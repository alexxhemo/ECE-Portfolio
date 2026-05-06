from flask import Blueprint, request, jsonify
from flask_jwt_extended import get_jwt_identity

from app.db import get_db
from app.utils import buyer_required

cart_bp = Blueprint('cart', __name__)

def _image_url(image_path):
    if not image_path:
        return None
    return f"/static/uploads/{image_path}"


def _cart_item_payload(row):
    price = float(row['price'])
    quantity = int(row['quantity'])
    subtotal = price * quantity

    return {
        'id': row['id'],
        'buyer_id': row['buyer_id'],
        'product_id': row['product_id'],
        'quantity': quantity,
        'subtotal': subtotal,
        'product': {
            'id': row['product_id'],
            'seller_id': row['seller_id'],
            'name': row['name'],
            'description': row['description'],
            'price': price,
            'price_unit': row['price_unit'],
            'category': row['category'],
            'image_path': row['image_path'],
            'image_url': _image_url(row['image_path']),
            'available_qty': max(0, row['stock_qty'] - row['reserved_qty']),
        }
    }


@cart_bp.route('', methods=['GET'])
@buyer_required
def get_cart():
    """
    Get the authenticated buyer's current cart contents.
    ---
    tags:
      - Cart
    security:
      - Bearer: []
    responses:
      200:
        description: List of cart items with embedded product details.
        schema:
          type: array
          items:
            type: object
            properties:
              id:
                type: integer
              product_id:
                type: integer
              quantity:
                type: integer
              product:
                type: object
                properties:
                  name:
                    type: string
                  price:
                    type: number
                  available_qty:
                    type: integer
                  category:
                    type: string
      401:
        description: Authentication required.
      403:
        description: Buyers only.
    """
    buyer_id = int(get_jwt_identity())
    db = get_db()
    rows = db.execute(
        '''
        SELECT ci.id, ci.buyer_id, ci.product_id, ci.quantity,
               p.seller_id, p.name, p.description, p.price, p.price_unit, p.category, p.image_path,
               p.quantity AS stock_qty, p.reserved_qty
        FROM cart_items ci
        JOIN products p ON p.id = ci.product_id
        WHERE ci.buyer_id = ?
        ORDER BY ci.id ASC
        ''',
        (buyer_id,)
    ).fetchall()
    items = [_cart_item_payload(row) for row in rows]
    total = sum(item['subtotal'] for item in items)
    return jsonify({'items': items, 'total': total}), 200


@cart_bp.route('', methods=['POST'])
@buyer_required
def add_to_cart():
    """
    Add a product to the buyer's cart. If the product is already in the cart,
    the quantities are summed. Quantity cannot exceed available stock.
    ---
    tags:
      - Cart
    security:
      - Bearer: []
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          required:
            - product_id
            - quantity
          properties:
            product_id:
              type: integer
              example: 1
            quantity:
              type: integer
              minimum: 1
              example: 2
    responses:
      200:
        description: Cart item updated (product already existed in cart).
      201:
        description: Cart item added successfully.
      400:
        description: Missing fields, quantity <= 0, or exceeds available stock.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
      404:
        description: Product not found.
    """
    buyer_id = int(get_jwt_identity())
    data = request.get_json() or {}
    product_id = data.get('product_id')
    quantity = data.get('quantity')

    if product_id is None or quantity is None:
        return jsonify({'error': 'product_id and quantity are required'}), 400

    try:
        product_id = int(product_id)
        quantity = int(quantity)
    except (TypeError, ValueError):
        return jsonify({'error': 'product_id and quantity must be integers'}), 400

    if quantity <= 0:
        return jsonify({'error': 'quantity must be greater than 0'}), 400

    db = get_db()
    product = db.execute('SELECT * FROM products WHERE id = ?', (product_id,)).fetchone()
    if not product:
        return jsonify({'error': 'Product not found'}), 404

    existing = db.execute(
        'SELECT * FROM cart_items WHERE buyer_id = ? AND product_id = ?',
        (buyer_id, product_id)
    ).fetchone()
    new_qty = quantity + (existing['quantity'] if existing else 0)
    available_qty = product['quantity'] - product['reserved_qty']

    if new_qty > available_qty:
        return jsonify({'error': f'Only {available_qty} units available'}), 400

    if existing:
        db.execute(
            'UPDATE cart_items SET quantity = ? WHERE id = ?',
            (new_qty, existing['id'])
        )
        item_id = existing['id']
    else:
        cursor = db.execute(
            'INSERT INTO cart_items (buyer_id, product_id, quantity) VALUES (?, ?, ?)',
            (buyer_id, product_id, quantity)
        )
        item_id = cursor.lastrowid
    db.commit()

    row = db.execute(
        '''
        SELECT ci.id, ci.buyer_id, ci.product_id, ci.quantity,
               p.seller_id, p.name, p.description, p.price, p.price_unit, p.category, p.image_path,
               p.quantity AS stock_qty, p.reserved_qty
        FROM cart_items ci
        JOIN products p ON p.id = ci.product_id
        WHERE ci.id = ?
        ''',
        (item_id,)
    ).fetchone()
    return jsonify(_cart_item_payload(row)), 201 if not existing else 200


@cart_bp.route('/<int:item_id>', methods=['PUT'])
@buyer_required
def update_cart_item(item_id):
    """
    Update the quantity of a specific cart item. Quantity cannot exceed available stock.
    ---
    tags:
      - Cart
    security:
      - Bearer: []
    parameters:
      - in: path
        name: item_id
        type: integer
        required: true
        example: 1
      - in: body
        name: body
        required: true
        schema:
          type: object
          required:
            - quantity
          properties:
            quantity:
              type: integer
              minimum: 1
              example: 3
    responses:
      200:
        description: Cart item quantity updated.
      400:
        description: Invalid quantity or exceeds available stock.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
      404:
        description: Cart item not found.
    """
    buyer_id = int(get_jwt_identity())
    data = request.get_json() or {}
    quantity = data.get('quantity')
    try:
        quantity = int(quantity)
    except (TypeError, ValueError):
        return jsonify({'error': 'quantity must be an integer'}), 400

    if quantity <= 0:
        return jsonify({'error': 'quantity must be greater than 0'}), 400

    db = get_db()
    item = db.execute(
        'SELECT * FROM cart_items WHERE id = ? AND buyer_id = ?',
        (item_id, buyer_id)
    ).fetchone()
    if not item:
        return jsonify({'error': 'Cart item not found'}), 404

    product = db.execute('SELECT * FROM products WHERE id = ?', (item['product_id'],)).fetchone()
    available_qty = product['quantity'] - product['reserved_qty']
    if quantity > available_qty:
        return jsonify({'error': f'Only {available_qty} units available'}), 400

    db.execute('UPDATE cart_items SET quantity = ? WHERE id = ?', (quantity, item_id))
    db.commit()

    row = db.execute(
        '''
        SELECT ci.id, ci.buyer_id, ci.product_id, ci.quantity,
               p.seller_id, p.name, p.description, p.price, p.price_unit, p.category, p.image_path,
               p.quantity AS stock_qty, p.reserved_qty
        FROM cart_items ci
        JOIN products p ON p.id = ci.product_id
        WHERE ci.id = ?
        ''',
        (item_id,)
    ).fetchone()
    return jsonify(_cart_item_payload(row)), 200


@cart_bp.route('/<int:item_id>', methods=['DELETE'])
@buyer_required
def delete_cart_item(item_id):
    """
    Remove a specific item from the buyer's cart.
    ---
    tags:
      - Cart
    security:
      - Bearer: []
    parameters:
      - in: path
        name: item_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Cart item removed successfully.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
      404:
        description: Cart item not found.
    """
    buyer_id = int(get_jwt_identity())
    db = get_db()
    deleted = db.execute(
        'DELETE FROM cart_items WHERE id = ? AND buyer_id = ?',
        (item_id, buyer_id)
    )
    db.commit()
    if deleted.rowcount == 0:
        return jsonify({'error': 'Cart item not found'}), 404
    return jsonify({'message': 'Cart item removed'}), 200


@cart_bp.route('/clear', methods=['DELETE'])
@buyer_required
def clear_cart():
    """
    Remove all items from the authenticated buyer's cart.
    ---
    tags:
      - Cart
    security:
      - Bearer: []
    responses:
      200:
        description: Cart cleared successfully.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
    """
    buyer_id = int(get_jwt_identity())
    db = get_db()
    db.execute('DELETE FROM cart_items WHERE buyer_id = ?', (buyer_id,))
    db.commit()
    return jsonify({'message': 'Cart cleared'}), 200
