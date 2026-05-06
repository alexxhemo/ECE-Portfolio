import os

from flask import Blueprint, request, jsonify, current_app
from flask_jwt_extended import get_jwt_identity

from app.db import get_db
from app.utils import seller_required, save_upload

products_bp = Blueprint('products', __name__)

VALID_CATEGORIES = ['Produce', 'Dairy', 'Meat', 'Baked Goods', 'Fruit', 'Vegetables', 'Other']
VALID_PRICE_UNITS = ['piece', 'pound', 'gallon', 'dozen', 'bag', 'bunch']

def _image_url(image_path):
    if not image_path:
        return None
    return f"/static/uploads/{image_path}"

def _product_dict(row, seller=None):
    if not row:
        return None

    data = {
        'id': row['id'],
        'seller_id': row['seller_id'],
        'name': row['name'],
        'description': row['description'],
        'price': row['price'],
        'price_unit': row['price_unit'],
        'quantity': row['quantity'],
        'reserved_qty': row['reserved_qty'],
        'available_qty': max(0, row['quantity'] - row['reserved_qty']),
        'image_path': row['image_path'],
        'image_url': _image_url(row['image_path']),
        'category': row['category'],
        'created_at': row['created_at'],
        'updated_at': row['updated_at'],
        'farm_name': None,
        'zip_code': None,
        'pickup_address': None,
    }

    if seller:
        data['seller'] = seller
        data['farm_name'] = seller.get('farm_name')
        data['zip_code'] = seller.get('zip_code')
        data['pickup_address'] = seller.get('pickup_address')

    return data


def _seller_payload(db, seller_id):
    user = db.execute('SELECT id, email FROM users WHERE id = ?', (seller_id,)).fetchone()
    farm = db.execute(
        'SELECT farm_name, zip_code, pickup_address FROM farm_profiles WHERE user_id = ?',
        (seller_id,)
    ).fetchone()
    if not user:
        return None
    return {
        'id': user['id'],
        'email': user['email'],
        'farm_name': farm['farm_name'] if farm else None,
        'zip_code': farm['zip_code'] if farm else None,
        'pickup_address': farm['pickup_address'] if farm else None,
    }


@products_bp.route('', methods=['GET'])
def list_products():
    """
    List all available marketplace products with optional filters.
    ---
    tags:
      - Products
    parameters:
      - in: query
        name: category
        type: string
        description: Filter by category (Produce, Dairy, Meat, Baked Goods, Fruit, Vegetables, Other)
        example: Produce
      - in: query
        name: search
        type: string
        description: Keyword search on product name
        example: apple
      - in: query
        name: zip_code
        type: string
        description: Filter to products from farms in a specific zip code
        example: "06269"
      - in: query
        name: seller_id
        type: integer
        description: Filter to products from a specific seller
        example: 3
    responses:
      200:
        description: List of available products (quantity > reserved).
        schema:
          type: array
          items:
            type: object
            properties:
              id:
                type: integer
              name:
                type: string
              price:
                type: number
              available_qty:
                type: integer
              category:
                type: string
              seller:
                type: object
    """
    db = get_db()
    clauses = ['p.quantity > p.reserved_qty']
    params = []

    category = request.args.get('category')
    if category:
        clauses.append('LOWER(p.category) LIKE ?')
        params.append(f"%{category.lower()}%")

    search = request.args.get('search')
    if search:
        clauses.append('LOWER(p.name) LIKE ?')
        params.append(f"%{search.lower()}%")

    zip_code = request.args.get('zip_code')
    if zip_code:
        clauses.append('EXISTS (SELECT 1 FROM farm_profiles fp WHERE fp.user_id = p.seller_id AND fp.zip_code = ?)')
        params.append(zip_code)

    seller_id = request.args.get('seller_id')
    if seller_id:
        clauses.append('p.seller_id = ?')
        params.append(int(seller_id))

    query = f'''
        SELECT p.*
        FROM products p
        WHERE {' AND '.join(clauses)}
        ORDER BY p.created_at DESC, p.id DESC
    '''
    rows = db.execute(query, tuple(params)).fetchall()

    result = []
    for row in rows:
        result.append(_product_dict(row, _seller_payload(db, row['seller_id'])))
    return jsonify(result), 200


@products_bp.route('/mine', methods=['GET'])
@seller_required
def my_products():
    """
    Get all product listings belonging to the authenticated seller.
    ---
    tags:
      - Products
    security:
      - Bearer: []
    parameters:
      - in: query
        name: status
        type: string
        enum: [in_stock, out_of_stock]
        description: Filter by stock status
    responses:
      200:
        description: List of the seller's products.
        schema:
          type: array
          items:
            type: object
      401:
        description: Authentication required.
      403:
        description: Sellers only.
    """
    seller_id = int(get_jwt_identity())
    status = request.args.get('status')
    db = get_db()

    clauses = ['seller_id = ?']
    params = [seller_id]
    if status == 'in_stock':
        clauses.append('quantity > reserved_qty')
    elif status == 'out_of_stock':
        clauses.append('quantity <= reserved_qty')

    query = f'''
        SELECT * FROM products
        WHERE {' AND '.join(clauses)}
        ORDER BY created_at DESC, id DESC
    '''
    rows = db.execute(query, tuple(params)).fetchall()
    seller = _seller_payload(db, seller_id)
    return jsonify([_product_dict(row, seller) for row in rows]), 200


@products_bp.route('/<int:product_id>', methods=['GET'])
def get_product(product_id):
    """
    Get details for a single product by ID.
    ---
    tags:
      - Products
    parameters:
      - in: path
        name: product_id
        type: integer
        required: true
        description: The product ID
        example: 1
    responses:
      200:
        description: Product detail including seller info.
        schema:
          type: object
          properties:
            id:
              type: integer
            name:
              type: string
            price:
              type: number
            available_qty:
              type: integer
            category:
              type: string
            seller:
              type: object
      404:
        description: Product not found.
    """
    db = get_db()
    row = db.execute('SELECT * FROM products WHERE id = ?', (product_id,)).fetchone()
    if not row:
        return jsonify({'error': 'Product not found'}), 404
    return jsonify(_product_dict(row, _seller_payload(db, row['seller_id']))), 200


@products_bp.route('', methods=['POST'])
@seller_required
def create_product():
    """
    Create a new product listing. Accepts JSON or multipart/form-data (for image upload).
    ---
    tags:
      - Products
    security:
      - Bearer: []
    consumes:
      - application/json
      - multipart/form-data
    parameters:
      - in: formData
        name: name
        type: string
        required: true
        example: Fresh Apples
      - in: formData
        name: description
        type: string
        example: Picked this morning
      - in: formData
        name: price
        type: number
        required: true
        example: 3.99
      - in: formData
        name: quantity
        type: integer
        required: true
        example: 50
      - in: formData
        name: category
        type: string
        enum: [Produce, Dairy, Meat, Baked Goods, Fruit, Vegetables, Other]
        example: Produce
      - in: formData
        name: image
        type: file
        description: Optional product image (png, jpg, jpeg, gif, webp, max 8MB)
    responses:
      201:
        description: Product created successfully.
      400:
        description: Missing required fields or invalid values.
      401:
        description: Authentication required.
      403:
        description: Sellers only.
    """
    seller_id = int(get_jwt_identity())
    json_data = request.get_json(silent=True) or {}

    name = request.form.get('name') or json_data.get('name')
    description = request.form.get('description', json_data.get('description', ''))
    price = request.form.get('price') or json_data.get('price')
    quantity = request.form.get('quantity') or json_data.get('quantity')
    category = request.form.get('category') or json_data.get('category', 'Other')
    price_unit = request.form.get('price_unit') or json_data.get('price_unit', 'piece')

    if not name or price is None or quantity is None:
        return jsonify({'error': 'name, price and quantity are required'}), 400

    try:
        price = float(price)
        quantity = int(quantity)
    except (ValueError, TypeError):
        return jsonify({'error': 'price must be a number and quantity must be an integer'}), 400

    if price < 0:
        return jsonify({'error': 'price must be non-negative'}), 400
    if quantity < 0:
        return jsonify({'error': 'quantity must be non-negative'}), 400

    if category not in VALID_CATEGORIES:
        category = 'Other'

    if price_unit not in VALID_PRICE_UNITS:
        price_unit = 'piece'

    image_filename = None
    if 'image' in request.files:
        image_filename = save_upload(request.files['image'])

    db = get_db()
    cursor = db.execute(
        '''
        INSERT INTO products (seller_id, name, description, price, quantity, reserved_qty, image_path, category, price_unit)
        VALUES (?, ?, ?, ?, ?, 0, ?, ?, ?)
        ''',
        (seller_id, name, description, price, quantity, image_filename, category, price_unit)
    )
    db.commit()

    row = db.execute('SELECT * FROM products WHERE id = ?', (cursor.lastrowid,)).fetchone()
    return jsonify(_product_dict(row, _seller_payload(db, seller_id))), 201


@products_bp.route('/<int:product_id>', methods=['PUT'])
@seller_required
def update_product(product_id):
    """
    Update an existing product listing. Only the owning seller can update.
    ---
    tags:
      - Products
    security:
      - Bearer: []
    consumes:
      - application/json
      - multipart/form-data
    parameters:
      - in: path
        name: product_id
        type: integer
        required: true
        example: 1
      - in: formData
        name: name
        type: string
        example: Fresh Apples
      - in: formData
        name: description
        type: string
        example: Picked this morning
      - in: formData
        name: price
        type: number
        example: 4.50
      - in: formData
        name: quantity
        type: integer
        example: 40
      - in: formData
        name: category
        type: string
        enum: [Produce, Dairy, Meat, Baked Goods, Fruit, Vegetables, Other]
      - in: formData
        name: image
        type: file
        description: Replacement product image (optional)
    responses:
      200:
        description: Product updated successfully.
      400:
        description: Invalid field values or quantity below reserved amount.
      403:
        description: Not your product.
      404:
        description: Product not found.
    """
    seller_id = int(get_jwt_identity())
    db = get_db()
    existing = db.execute('SELECT * FROM products WHERE id = ?', (product_id,)).fetchone()
    if not existing:
        return jsonify({'error': 'Product not found'}), 404
    if existing['seller_id'] != seller_id:
        return jsonify({'error': 'Not your product'}), 403

    data = request.form if request.content_type and 'multipart' in request.content_type else (request.get_json() or {})

    name = data.get('name', existing['name'])
    description = data.get('description', existing['description'])
    price = existing['price']
    price_unit = data.get('price_unit', existing['price_unit'])
    quantity = existing['quantity']
    category = data.get('category', existing['category'])
    image_path = existing['image_path']

    if 'price' in data:
        try:
            price = float(data['price'])
        except (ValueError, TypeError):
            return jsonify({'error': 'price must be a number'}), 400
        if price < 0:
            return jsonify({'error': 'price must be non-negative'}), 400
        
    if price_unit not in VALID_PRICE_UNITS:
      price_unit = 'piece'

    if 'quantity' in data:
        try:
            quantity = int(data['quantity'])
        except (ValueError, TypeError):
            return jsonify({'error': 'quantity must be an integer'}), 400
        if quantity < 0:
            return jsonify({'error': 'quantity must be non-negative'}), 400
        if quantity < existing['reserved_qty']:
            return jsonify({'error': 'quantity cannot be less than reserved quantity'}), 400

    if category not in VALID_CATEGORIES:
        category = 'Other'

    if 'image' in request.files:
        filename = save_upload(request.files['image'])
        if filename:
            if image_path:
                old_path = os.path.join(current_app.config['UPLOAD_FOLDER'], image_path)
                if os.path.exists(old_path):
                    os.remove(old_path)
            image_path = filename

    db.execute(
        '''
        UPDATE products
        SET name = ?, description = ?, price = ?, quantity = ?, image_path = ?, category = ?, price_unit = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
        ''',
        (name, description, price, quantity, image_path, category, price_unit, product_id)
    )
    db.commit()

    row = db.execute('SELECT * FROM products WHERE id = ?', (product_id,)).fetchone()
    return jsonify(_product_dict(row, _seller_payload(db, seller_id))), 200


@products_bp.route('/<int:product_id>', methods=['DELETE'])
@seller_required
def delete_product(product_id):
    """
    Delete a product listing. Only the owning seller can delete.
    Products with reserved inventory cannot be deleted.
    ---
    tags:
      - Products
    security:
      - Bearer: []
    parameters:
      - in: path
        name: product_id
        type: integer
        required: true
        example: 1
    responses:
      200:
        description: Product deleted successfully.
      400:
        description: Cannot delete a product with reserved inventory.
      403:
        description: Not your product.
      404:
        description: Product not found.
    """
    seller_id = int(get_jwt_identity())
    db = get_db()
    product = db.execute('SELECT * FROM products WHERE id = ?', (product_id,)).fetchone()
    if not product:
        return jsonify({'error': 'Product not found'}), 404
    if product['seller_id'] != seller_id:
        return jsonify({'error': 'Not your product'}), 403

    if product['reserved_qty'] > 0:
        return jsonify({'error': 'Cannot delete a product with reserved inventory'}), 400

    if product['image_path']:
        path = os.path.join(current_app.config['UPLOAD_FOLDER'], product['image_path'])
        if os.path.exists(path):
            os.remove(path)

    db.execute('DELETE FROM products WHERE id = ?', (product_id,))
    db.commit()
    return jsonify({'message': 'Product deleted'}), 200
