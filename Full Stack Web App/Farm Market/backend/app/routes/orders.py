from flask import Blueprint, request, jsonify
from flask_jwt_extended import get_jwt_identity

from app.db import get_db
from app.utils import buyer_required, seller_required, login_required

orders_bp = Blueprint('orders', __name__)

ORDER_STATUSES = {'pending', 'confirmed', 'ready_for_pickup', 'completed', 'rejected', 'cancelled'}

def _image_url(image_path):
    if not image_path:
        return None
    return f"/static/uploads/{image_path}"

def _review_payload(db, order_id):
    row = db.execute(
        '''
        SELECT id, buyer_id, seller_id, order_id, rating, comment, created_at
        FROM reviews
        WHERE order_id = ?
        ''',
        (order_id,)
    ).fetchone()

    if not row:
        return None

    return {
        'id': row['id'],
        'buyer_id': row['buyer_id'],
        'seller_id': row['seller_id'],
        'order_id': row['order_id'],
        'rating': row['rating'],
        'comment': row['comment'],
        'created_at': row['created_at'],
    }

def _fetch_order_items(db, order_id):
    rows = db.execute(
        '''
        SELECT oi.id, oi.order_id, oi.product_id, oi.quantity, oi.price_at_purchase,
               p.name, p.description, p.image_path, p.category, p.price_unit
        FROM order_items oi
        JOIN products p ON p.id = oi.product_id
        WHERE oi.order_id = ?
        ORDER BY oi.id ASC
        ''',
        (order_id,)
    ).fetchall()

    items = []
    for row in rows:
        price = float(row['price_at_purchase'])
        quantity = int(row['quantity'])
        subtotal = price * quantity

        items.append({
            'id': row['id'],
            'order_id': row['order_id'],
            'product_id': row['product_id'],
            'product_name': row['name'],
            'product_image_url': _image_url(row['image_path']),
            'quantity': quantity,
            'price_at_purchase': price,
            'subtotal': subtotal,
            'price_unit': row['price_unit'],

            # keep nested product too, just in case anything still uses it
            'product': {
                'id': row['product_id'],
                'name': row['name'],
                'description': row['description'],
                'image_path': row['image_path'],
                'image_url': _image_url(row['image_path']),
                'category': row['category'],
            }
        })

    return items


def _fetch_order(db, order_id):
    order = db.execute(
        'SELECT * FROM orders WHERE id = ?',
        (order_id,)
    ).fetchone()

    if not order:
        return None

    buyer = db.execute(
        '''
        SELECT full_name
        FROM buyer_profiles
        WHERE user_id = ?
        ''',
        (order['buyer_id'],)
    ).fetchone()

    farm = db.execute(
        '''
        SELECT farm_name, pickup_address
        FROM farm_profiles
        WHERE user_id = ?
        ''',
        (order['seller_id'],)
    ).fetchone()

    items = _fetch_order_items(db, order_id)
    total = sum(item['subtotal'] for item in items)

    return {
        'id': order['id'],
        'buyer_id': order['buyer_id'],
        'buyer_name': buyer['full_name'] if buyer else None,
        'seller_id': order['seller_id'],
        'farm_name': farm['farm_name'] if farm else None,
        'pickup_address': farm['pickup_address'] if farm else None,
        'status': order['status'],
        'created_at': order['created_at'],
        'updated_at': order['updated_at'],
        'total': total,
        'items': items,
        'review': _review_payload(db, order_id),
    }


def _release_inventory(db, order_id):
    items = db.execute(
        'SELECT product_id, quantity FROM order_items WHERE order_id = ?',
        (order_id,)
    ).fetchall()
    for item in items:
        db.execute(
            '''
            UPDATE products
            SET reserved_qty = CASE
                WHEN reserved_qty - ? < 0 THEN 0
                ELSE reserved_qty - ?
            END,
            updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            ''',
            (item['quantity'], item['quantity'], item['product_id'])
        )


@orders_bp.route('', methods=['POST'])
@buyer_required
def place_order():
    """
    Place an order from the buyer's current cart. Creates one order per seller.
    Inventory is soft-reserved (pending status) until the seller approves.
    Cart is cleared after successful placement.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    responses:
      201:
        description: Order(s) placed. Returns list of created orders with items.
        schema:
          type: object
          properties:
            message:
              type: string
              example: 1 order(s) placed
            orders:
              type: array
              items:
                type: object
                properties:
                  id:
                    type: integer
                  status:
                    type: string
                    example: pending
                  items:
                    type: array
      400:
        description: Cart is empty or insufficient stock for one or more items.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
    """
    buyer_id = int(get_jwt_identity())
    db = get_db()
    cart_items = db.execute(
        '''
        SELECT ci.id, ci.product_id, ci.quantity AS cart_qty,
               p.seller_id, p.name, p.price, p.quantity, p.reserved_qty
        FROM cart_items ci
        JOIN products p ON p.id = ci.product_id
        WHERE ci.buyer_id = ?
        ORDER BY p.seller_id, ci.id
        ''',
        (buyer_id,)
    ).fetchall()

    if not cart_items:
        return jsonify({'error': 'Cart is empty'}), 400

    for item in cart_items:
        available_qty = item['quantity'] - item['reserved_qty']
        if item['cart_qty'] > available_qty:
            return jsonify({'error': f"\"{item['name']}\" only has {available_qty} units available"}), 400

    seller_groups = {}
    for item in cart_items:
        seller_groups.setdefault(item['seller_id'], []).append(item)

    created_order_ids = []
    for seller_id, items in seller_groups.items():
        order_cursor = db.execute(
            'INSERT INTO orders (buyer_id, seller_id, status) VALUES (?, ?, ?)',
            (buyer_id, seller_id, 'pending')
        )
        order_id = order_cursor.lastrowid
        created_order_ids.append(order_id)

        for item in items:
            db.execute(
                '''
                INSERT INTO order_items (order_id, product_id, quantity, price_at_purchase)
                VALUES (?, ?, ?, ?)
                ''',
                (order_id, item['product_id'], item['cart_qty'], item['price'])
            )
            db.execute(
                '''
                UPDATE products
                SET reserved_qty = reserved_qty + ?, updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                ''',
                (item['cart_qty'], item['product_id'])
            )

    db.execute('DELETE FROM cart_items WHERE buyer_id = ?', (buyer_id,))
    db.commit()

    return jsonify({
        'message': f'{len(created_order_ids)} order(s) placed',
        'orders': [_fetch_order(db, order_id) for order_id in created_order_ids],
    }), 201


@orders_bp.route('/mine', methods=['GET'])
@buyer_required
def buyer_orders():
    """
    List all orders placed by the authenticated buyer.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: query
        name: status
        type: string
        enum: [pending, confirmed, ready_for_pickup, completed, rejected, cancelled]
        description: Filter orders by status
    responses:
      200:
        description: List of the buyer's orders with items.
        schema:
          type: array
          items:
            type: object
      400:
        description: Invalid status value.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
    """
    buyer_id = int(get_jwt_identity())
    status = request.args.get('status')
    db = get_db()

    if status and status not in ORDER_STATUSES:
        return jsonify({'error': 'Invalid status'}), 400

    if status:
        rows = db.execute(
            'SELECT id FROM orders WHERE buyer_id = ? AND status = ? ORDER BY created_at DESC, id DESC',
            (buyer_id, status)
        ).fetchall()
    else:
        rows = db.execute(
            'SELECT id FROM orders WHERE buyer_id = ? ORDER BY created_at DESC, id DESC',
            (buyer_id,)
        ).fetchall()

    return jsonify([_fetch_order(db, row['id']) for row in rows]), 200


@orders_bp.route('/incoming', methods=['GET'])
@seller_required
def incoming_orders():
    """
    List all orders received by the authenticated seller (Incoming Requests dashboard).
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: query
        name: status
        type: string
        enum: [pending, confirmed, ready_for_pickup, completed, rejected, cancelled]
        description: Filter orders by status. Omit to get all orders.
    responses:
      200:
        description: List of incoming orders with items and buyer info.
        schema:
          type: array
          items:
            type: object
      400:
        description: Invalid status value.
      401:
        description: Authentication required.
      403:
        description: Sellers only.
    """
    seller_id = int(get_jwt_identity())
    status = request.args.get('status')
    db = get_db()

    if status and status not in ORDER_STATUSES:
        return jsonify({'error': 'Invalid status'}), 400

    if status:
        rows = db.execute(
            'SELECT id FROM orders WHERE seller_id = ? AND status = ? ORDER BY created_at DESC, id DESC',
            (seller_id, status)
        ).fetchall()
    else:
        rows = db.execute(
            'SELECT id FROM orders WHERE seller_id = ? ORDER BY created_at DESC, id DESC',
            (seller_id,)
        ).fetchall()

    return jsonify([_fetch_order(db, row['id']) for row in rows]), 200


@orders_bp.route('/<int:order_id>', methods=['GET'])
@login_required
def get_order(order_id):
    """
    Get full details for a single order. Accessible by the buyer or seller of the order.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: path
        name: order_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Order detail with items.
      401:
        description: Authentication required.
      403:
        description: Not your order.
      404:
        description: Order not found.
    """
    user_id = int(get_jwt_identity())
    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404
    if order['buyer_id'] != user_id and order['seller_id'] != user_id:
        return jsonify({'error': 'Not your order'}), 403
    return jsonify(_fetch_order(db, order_id)), 200


@orders_bp.route('/<int:order_id>/cancel', methods=['POST'])
@buyer_required
def cancel_order(order_id):
    """
    Cancel an order. Only the buyer can cancel. Only pending or confirmed orders can be cancelled.
    Cancelling a pending order releases reserved inventory back to the listing.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: path
        name: order_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Order cancelled. Returns updated order.
      400:
        description: Order is not in a cancellable state.
      401:
        description: Authentication required.
      403:
        description: Not your order or buyers only.
      404:
        description: Order not found.
    """
    buyer_id = int(get_jwt_identity())
    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404
    if order['buyer_id'] != buyer_id:
        return jsonify({'error': 'Not your order'}), 403
    if order['status'] not in ('pending', 'confirmed'):
        return jsonify({'error': f"Cannot cancel a {order['status']} order"}), 400

    if order['status'] == 'pending':
        _release_inventory(db, order_id)
    db.execute(
        'UPDATE orders SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?',
        ('cancelled', order_id)
    )
    db.commit()
    return jsonify(_fetch_order(db, order_id)), 200


@orders_bp.route('/<int:order_id>/approve', methods=['POST'])
@seller_required
def approve_order(order_id):
    """
    Approve a pending order. Only the owning seller can approve.
    Permanently deducts inventory from the listing (reserved_qty and quantity both decrease).
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: path
        name: order_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Order approved and moved to confirmed status.
      400:
        description: Order is not pending.
      401:
        description: Authentication required.
      403:
        description: Not your order or sellers only.
      404:
        description: Order not found.
    """
    seller_id = int(get_jwt_identity())
    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404
    if order['seller_id'] != seller_id:
        return jsonify({'error': 'Not your order'}), 403
    if order['status'] != 'pending':
        return jsonify({'error': 'Only pending orders can be approved'}), 400

    items = db.execute(
        'SELECT product_id, quantity FROM order_items WHERE order_id = ?',
        (order_id,)
    ).fetchall()
    for item in items:
        db.execute(
            '''
            UPDATE products
            SET quantity = quantity - ?,
                reserved_qty = reserved_qty - ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            ''',
            (item['quantity'], item['quantity'], item['product_id'])
        )

    db.execute(
        'UPDATE orders SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?',
        ('confirmed', order_id)
    )
    db.commit()
    return jsonify(_fetch_order(db, order_id)), 200


@orders_bp.route('/<int:order_id>/reject', methods=['POST'])
@seller_required
def reject_order(order_id):
    """
    Reject a pending order. Only the owning seller can reject.
    Releases all reserved inventory back to the listing.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: path
        name: order_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Order rejected and inventory released.
      400:
        description: Order is not pending.
      401:
        description: Authentication required.
      403:
        description: Not your order or sellers only.
      404:
        description: Order not found.
    """
    seller_id = int(get_jwt_identity())
    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404
    if order['seller_id'] != seller_id:
        return jsonify({'error': 'Not your order'}), 403
    if order['status'] != 'pending':
        return jsonify({'error': 'Only pending orders can be rejected'}), 400

    _release_inventory(db, order_id)
    db.execute(
        'UPDATE orders SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?',
        ('rejected', order_id)
    )
    db.commit()
    return jsonify(_fetch_order(db, order_id)), 200


@orders_bp.route('/<int:order_id>/ready', methods=['POST'])
@seller_required
def mark_ready(order_id):
    """
    Mark a confirmed order as ready for pickup. Only the owning seller can mark ready.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: path
        name: order_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Order marked as ready_for_pickup.
      400:
        description: Order is not confirmed.
      401:
        description: Authentication required.
      403:
        description: Not your order or sellers only.
      404:
        description: Order not found.
    """
    seller_id = int(get_jwt_identity())
    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404
    if order['seller_id'] != seller_id:
        return jsonify({'error': 'Not your order'}), 403
    if order['status'] != 'confirmed':
        return jsonify({'error': 'Only confirmed orders can be marked ready'}), 400

    db.execute(
        'UPDATE orders SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?',
        ('ready_for_pickup', order_id)
    )
    db.commit()
    return jsonify(_fetch_order(db, order_id)), 200


@orders_bp.route('/<int:order_id>/complete', methods=['POST'])
@login_required
def complete_order(order_id):
    """
    Mark an order as completed after physical pickup. Accessible by either the buyer or seller.
    Order must be in ready_for_pickup status. After completion, the buyer may leave a review.
    ---
    tags:
      - Orders
    security:
      - Bearer: []
    parameters:
      - in: path
        name: order_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Order marked as completed.
      400:
        description: Order is not in ready_for_pickup status.
      401:
        description: Authentication required.
      403:
        description: Not your order.
      404:
        description: Order not found.
    """
    user_id = int(get_jwt_identity())
    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404

    user = db.execute('SELECT id, role FROM users WHERE id = ?', (user_id,)).fetchone()
    allowed = (
        (user['role'] == 'seller' and order['seller_id'] == user_id) or
        (user['role'] == 'buyer' and order['buyer_id'] == user_id)
    )
    if not allowed:
        return jsonify({'error': 'Not your order'}), 403
    if order['status'] != 'ready_for_pickup':
        return jsonify({'error': 'Order must be ready_for_pickup to complete'}), 400

    db.execute(
        'UPDATE orders SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?',
        ('completed', order_id)
    )
    db.commit()
    return jsonify(_fetch_order(db, order_id)), 200
