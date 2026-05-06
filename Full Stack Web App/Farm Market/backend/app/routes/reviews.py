from flask import Blueprint, request, jsonify
from flask_jwt_extended import get_jwt_identity

from app.db import get_db
from app.utils import buyer_required

reviews_bp = Blueprint('reviews', __name__)


def _review_payload(row, buyer_name=None, farm_name=None):
    return {
        'id': row['id'],
        'buyer_id': row['buyer_id'],
        'buyer_name': buyer_name,
        'seller_id': row['seller_id'],
        'farm_name': farm_name,
        'order_id': row['order_id'],
        'rating': row['rating'],
        'comment': row['comment'],
        'created_at': row['created_at'],
    }


@reviews_bp.route('', methods=['POST'])
@buyer_required
def create_review():
    """
    Submit a review for a seller. Only allowed after the related order is completed.
    One review per order is enforced.
    ---
    tags:
      - Reviews
    security:
      - Bearer: []
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          required:
            - order_id
            - rating
          properties:
            order_id:
              type: integer
              description: ID of a completed order placed by this buyer
              example: 1
            rating:
              type: integer
              minimum: 1
              maximum: 5
              description: Star rating from 1 to 5
              example: 5
            comment:
              type: string
              description: Optional text comment
              example: Fresh and delicious!
    responses:
      201:
        description: Review submitted successfully.
        schema:
          type: object
          properties:
            id:
              type: integer
            buyer_id:
              type: integer
            seller_id:
              type: integer
            order_id:
              type: integer
            rating:
              type: integer
            comment:
              type: string
            created_at:
              type: string
      400:
        description: Missing fields, invalid rating range, or order is not completed.
      401:
        description: Authentication required.
      403:
        description: Buyers only, or the order does not belong to this buyer.
      404:
        description: Order not found.
      409:
        description: A review already exists for this order.
    """
    buyer_id = int(get_jwt_identity())
    data = request.get_json() or {}
    order_id = data.get('order_id')
    rating = data.get('rating')
    comment = data.get('comment', '')

    if order_id is None or rating is None:
        return jsonify({'error': 'order_id and rating are required'}), 400

    try:
        order_id = int(order_id)
        rating = int(rating)
    except (TypeError, ValueError):
        return jsonify({'error': 'order_id and rating must be integers'}), 400

    if not (1 <= rating <= 5):
        return jsonify({'error': 'Rating must be between 1 and 5'}), 400

    db = get_db()
    order = db.execute('SELECT * FROM orders WHERE id = ?', (order_id,)).fetchone()
    if not order:
        return jsonify({'error': 'Order not found'}), 404
    if order['buyer_id'] != buyer_id:
        return jsonify({'error': 'Not your order'}), 403
    if order['status'] != 'completed':
        return jsonify({'error': 'You can only review completed orders'}), 400

    existing = db.execute('SELECT id FROM reviews WHERE order_id = ?', (order_id,)).fetchone()
    if existing:
        return jsonify({'error': 'You have already reviewed this order'}), 409

    cursor = db.execute(
        '''
        INSERT INTO reviews (buyer_id, seller_id, order_id, rating, comment)
        VALUES (?, ?, ?, ?, ?)
        ''',
        (buyer_id, order['seller_id'], order_id, rating, comment)
    )
    db.commit()

    review = db.execute('SELECT * FROM reviews WHERE id = ?', (cursor.lastrowid,)).fetchone()
    buyer = db.execute(
        '''
        SELECT full_name
        FROM buyer_profiles
        WHERE user_id = ?
        ''',
        (buyer_id,)
    ).fetchone()

    farm = db.execute(
        '''
        SELECT farm_name
        FROM farm_profiles
        WHERE user_id = ?
        ''',
        (order['seller_id'],)
    ).fetchone()
    return jsonify(_review_payload(review, buyer['full_name'] if buyer else None, farm['farm_name'] if farm else None)), 201


@reviews_bp.route('/seller/<int:seller_id>', methods=['GET'])
def seller_reviews(seller_id):
    """
    Get all reviews for a seller, including average rating. Public endpoint.
    ---
    tags:
      - Reviews
    parameters:
      - in: path
        name: seller_id
        type: integer
        required: true
        description: The seller's user ID
        example: 2
    responses:
      200:
        description: Seller's reviews and aggregate rating.
        schema:
          type: object
          properties:
            seller_id:
              type: integer
            avg_rating:
              type: number
              description: Average star rating (null if no reviews)
              example: 4.5
            review_count:
              type: integer
              example: 12
            reviews:
              type: array
              items:
                type: object
    """
    db = get_db()
    rows = db.execute(
        '''
        SELECT r.*, bp.full_name AS buyer_name
        FROM reviews r
        LEFT JOIN buyer_profiles bp ON bp.user_id = r.buyer_id
        WHERE r.seller_id = ?
        ORDER BY r.created_at DESC, r.id DESC
        ''',
        (seller_id,)
    ).fetchall()
    if rows:
        avg_rating = round(sum(row['rating'] for row in rows) / len(rows), 1)
    else:
        avg_rating = None

    return jsonify({
        'seller_id': seller_id,
        'avg_rating': avg_rating,
        'review_count': len(rows),
        'reviews': [
            _review_payload(
                row,
                buyer_name=row['buyer_name'],
                farm_name=None,
            )
            for row in rows
        ],
    }), 200


@reviews_bp.route('/mine', methods=['GET'])
@buyer_required
def my_reviews():
    """
    Get all reviews submitted by the authenticated buyer.
    ---
    tags:
      - Reviews
    security:
      - Bearer: []
    responses:
      200:
        description: List of reviews written by this buyer, newest first.
        schema:
          type: array
          items:
            type: object
            properties:
              id:
                type: integer
              seller_id:
                type: integer
              order_id:
                type: integer
              rating:
                type: integer
              comment:
                type: string
              created_at:
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
        SELECT r.*, fp.farm_name AS farm_name
        FROM reviews r
        LEFT JOIN farm_profiles fp ON fp.user_id = r.seller_id
        WHERE r.buyer_id = ?
        ORDER BY r.created_at DESC, r.id DESC
        ''',
        (buyer_id,)
    ).fetchall()

    return jsonify([
        _review_payload(
            row,
            buyer_name=None,
            farm_name=row['farm_name'],
        )
        for row in rows
    ]), 200
