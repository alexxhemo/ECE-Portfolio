from flask import Blueprint, request, jsonify
from flask_jwt_extended import get_jwt_identity

from app.db import get_db
from app.utils import buyer_required, seller_required

profile_bp = Blueprint('profile', __name__)


def _buyer_profile_dict(row):
    if not row:
        return None
    return {
        'id': row['id'],
        'user_id': row['user_id'],
        'full_name': row['full_name'],
        'phone': row['phone'],
        'zip_code': row['zip_code'],
    }


def _seller_profile_dict(row):
    if not row:
        return None
    return {
        'id': row['id'],
        'user_id': row['user_id'],
        'farm_name': row['farm_name'],
        'biography': row['biography'],
        'pickup_address': row['pickup_address'],
        'operating_hours': row['operating_hours'],
        'zip_code': row['zip_code'],
    }


@profile_bp.route('/buyer', methods=['GET'])
@buyer_required
def get_buyer_profile():
    """
    Get the authenticated buyer's profile (contact info and home zip code).
    ---
    tags:
      - Profile
    security:
      - Bearer: []
    responses:
      200:
        description: Buyer's profile data.
        schema:
          type: object
          properties:
            id:
              type: integer
            user_id:
              type: integer
            full_name:
              type: string
            phone:
              type: string
            zip_code:
              type: string
      401:
        description: Authentication required.
      403:
        description: Buyers only.
      404:
        description: Profile not found.
    """
    user_id = int(get_jwt_identity())
    db = get_db()
    profile = db.execute(
        'SELECT * FROM buyer_profiles WHERE user_id = ?',
        (user_id,)
    ).fetchone()

    if not profile:
        return jsonify({'error': 'Profile not found'}), 404

    return jsonify(_buyer_profile_dict(profile)), 200


@profile_bp.route('/buyer', methods=['PUT'])
@buyer_required
def update_buyer_profile():
    """
    Update the authenticated buyer's profile (contact info and home zip code).
    ---
    tags:
      - Profile
    security:
      - Bearer: []
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          properties:
            full_name:
              type: string
              example: Jane Smith
            phone:
              type: string
              example: "555-1234"
            zip_code:
              type: string
              description: Home zip code used for location-based search
              example: "06269"
    responses:
      200:
        description: Buyer profile updated successfully.
      401:
        description: Authentication required.
      403:
        description: Buyers only.
    """
    user_id = int(get_jwt_identity())
    data = request.get_json() or {}
    db = get_db()

    existing = db.execute(
        'SELECT id FROM buyer_profiles WHERE user_id = ?',
        (user_id,)
    ).fetchone()

    if existing:
        db.execute(
            '''
            UPDATE buyer_profiles
            SET full_name = ?, phone = ?, zip_code = ?
            WHERE user_id = ?
            ''',
            (
                data.get('full_name', ''),
                data.get('phone', ''),
                data.get('zip_code', ''),
                user_id,
            )
        )
    else:
        db.execute(
            '''
            INSERT INTO buyer_profiles (user_id, full_name, phone, zip_code)
            VALUES (?, ?, ?, ?)
            ''',
            (
                user_id,
                data.get('full_name', ''),
                data.get('phone', ''),
                data.get('zip_code', ''),
            )
        )
    db.commit()

    profile = db.execute(
        'SELECT * FROM buyer_profiles WHERE user_id = ?',
        (user_id,)
    ).fetchone()
    return jsonify(_buyer_profile_dict(profile)), 200


@profile_bp.route('/seller', methods=['GET'])
@seller_required
def get_seller_profile():
    """
    Get the authenticated seller's farm profile (private view).
    ---
    tags:
      - Profile
    security:
      - Bearer: []
    responses:
      200:
        description: The seller's farm profile.
        schema:
          type: object
          properties:
            id:
              type: integer
            user_id:
              type: integer
            farm_name:
              type: string
            biography:
              type: string
            pickup_address:
              type: string
            operating_hours:
              type: string
            zip_code:
              type: string
      401:
        description: Authentication required.
      403:
        description: Sellers only.
      404:
        description: Profile not found.
    """
    user_id = int(get_jwt_identity())
    db = get_db()
    profile = db.execute(
        'SELECT * FROM farm_profiles WHERE user_id = ?',
        (user_id,)
    ).fetchone()

    if not profile:
        return jsonify({'error': 'Profile not found'}), 404

    return jsonify(_seller_profile_dict(profile)), 200


@profile_bp.route('/seller', methods=['PUT'])
@seller_required
def update_seller_profile():
    """
    Update the authenticated seller's farm profile.
    ---
    tags:
      - Profile
    security:
      - Bearer: []
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          properties:
            farm_name:
              type: string
              example: Cheshire Farms
            biography:
              type: string
              example: Family farm since 1980
            pickup_address:
              type: string
              example: 123 Farm Rd, Storrs CT
            operating_hours:
              type: string
              example: Mon-Sat 8am-5pm
            zip_code:
              type: string
              example: "06269"
    responses:
      200:
        description: Farm profile updated successfully.
      401:
        description: Authentication required.
      403:
        description: Sellers only.
    """
    user_id = int(get_jwt_identity())
    data = request.get_json() or {}
    db = get_db()

    existing = db.execute(
        'SELECT id FROM farm_profiles WHERE user_id = ?',
        (user_id,)
    ).fetchone()

    values = (
        data.get('farm_name', ''),
        data.get('biography', ''),
        data.get('pickup_address', ''),
        data.get('operating_hours', ''),
        data.get('zip_code', ''),
        user_id,
    )

    if existing:
        db.execute(
            '''
            UPDATE farm_profiles
            SET farm_name = ?, biography = ?, pickup_address = ?, operating_hours = ?, zip_code = ?
            WHERE user_id = ?
            ''',
            values,
        )
    else:
        db.execute(
            '''
            INSERT INTO farm_profiles (farm_name, biography, pickup_address, operating_hours, zip_code, user_id)
            VALUES (?, ?, ?, ?, ?, ?)
            ''',
            values,
        )
    db.commit()

    profile = db.execute(
        'SELECT * FROM farm_profiles WHERE user_id = ?',
        (user_id,)
    ).fetchone()
    return jsonify(_seller_profile_dict(profile)), 200


@profile_bp.route('/farm/<int:seller_id>', methods=['GET'])
def get_public_farm_profile(seller_id):
    """
    Get the public farm profile page for a seller, including average rating.
    ---
    tags:
      - Profile
    parameters:
      - in: path
        name: seller_id
        type: integer
        required: true
        description: The seller's user ID
        example: 2
    responses:
      200:
        description: Public farm profile with reviews summary.
        schema:
          type: object
          properties:
            user_id:
              type: integer
            email:
              type: string
            farm_name:
              type: string
            biography:
              type: string
            pickup_address:
              type: string
            operating_hours:
              type: string
            zip_code:
              type: string
            avg_rating:
              type: number
              description: Average star rating (null if no reviews)
            review_count:
              type: integer
      404:
        description: Seller not found.
    """
    db = get_db()
    user = db.execute(
        'SELECT id, email, role FROM users WHERE id = ?',
        (seller_id,)
    ).fetchone()

    if not user or user['role'] != 'seller':
        return jsonify({'error': 'Not a seller'}), 404

    farm = db.execute(
        'SELECT * FROM farm_profiles WHERE user_id = ?',
        (seller_id,)
    ).fetchone()
    reviews = db.execute(
        'SELECT rating FROM reviews WHERE seller_id = ?',
        (seller_id,)
    ).fetchall()

    result = _seller_profile_dict(farm) if farm else {'user_id': seller_id}
    result['email'] = user['email']
    if reviews:
        ratings = [row['rating'] for row in reviews]
        result['avg_rating'] = round(sum(ratings) / len(ratings), 1)
        result['review_count'] = len(ratings)
    else:
        result['avg_rating'] = None
        result['review_count'] = 0

    return jsonify(result), 200
