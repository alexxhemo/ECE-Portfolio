from datetime import datetime, timedelta, timezone

from flask import Blueprint, request, jsonify
from flask_jwt_extended import get_jwt_identity

from app.db import get_db
from app.utils import login_required

membership_bp = Blueprint('membership', __name__)

BUYER_PLANS = {'free': 0, 'starter': 9.99, 'pro_farmer': 29.99}
SELLER_PLANS = {'free': 0, 'pro': 9.99}


def _membership_payload(row):
    if not row:
        return None
    return {
        'id': row['id'],
        'user_id': row['user_id'],
        'plan': row['plan'],
        'started_at': row['started_at'],
        'expires_at': row['expires_at'],
    }


@membership_bp.route('', methods=['GET'])
@login_required
def get_membership():
    """
    Get the authenticated user's current membership plan.
    ---
    tags:
      - Membership
    security:
      - Bearer: []
    responses:
      200:
        description: Current membership details.
        schema:
          type: object
          properties:
            id:
              type: integer
            user_id:
              type: integer
            plan:
              type: string
              example: free
            started_at:
              type: string
            expires_at:
              type: string
              description: ISO timestamp, null for free plan
      401:
        description: Authentication required.
      404:
        description: No membership found.
    """
    user_id = int(get_jwt_identity())
    db = get_db()
    membership = db.execute(
        'SELECT * FROM memberships WHERE user_id = ?',
        (user_id,)
    ).fetchone()
    if not membership:
        return jsonify({'error': 'No membership found'}), 404
    return jsonify(_membership_payload(membership)), 200


@membership_bp.route('/upgrade', methods=['POST'])
@login_required
def upgrade_membership():
    """
    Upgrade or change the authenticated user's membership plan.
    Buyer plans: free, starter ($9.99/mo), pro_farmer ($29.99/mo).
    Seller plans: free, pro ($9.99/mo).
    Paid plans expire after 30 days. Downgrading to free sets expires_at to null.
    ---
    tags:
      - Membership
    security:
      - Bearer: []
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          required:
            - plan
          properties:
            plan:
              type: string
              description: "Buyer: free | starter | pro_farmer. Seller: free | pro"
              example: starter
    responses:
      200:
        description: Membership upgraded successfully.
        schema:
          type: object
          properties:
            message:
              type: string
              example: Upgraded to starter
            membership:
              type: object
            price:
              type: number
              example: 9.99
      400:
        description: Invalid plan name for this role.
      401:
        description: Authentication required.
      404:
        description: User not found.
    """
    user_id = int(get_jwt_identity())
    db = get_db()
    user = db.execute('SELECT id, role FROM users WHERE id = ?', (user_id,)).fetchone()
    if not user:
        return jsonify({'error': 'User not found'}), 404

    data = request.get_json() or {}
    plan = str(data.get('plan', '')).lower()
    valid_plans = BUYER_PLANS if user['role'] == 'buyer' else SELLER_PLANS

    if plan not in valid_plans:
        return jsonify({
            'error': f'Invalid plan. Valid options: {list(valid_plans.keys())}',
            'pricing': valid_plans,
        }), 400

    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    expires_at = None
    if plan != 'free':
        expires_at = (datetime.now(timezone.utc) + timedelta(days=30)).replace(microsecond=0).isoformat()

    existing = db.execute('SELECT id FROM memberships WHERE user_id = ?', (user_id,)).fetchone()
    if existing:
        db.execute(
            '''
            UPDATE memberships
            SET plan = ?, started_at = ?, expires_at = ?
            WHERE user_id = ?
            ''',
            (plan, now, expires_at, user_id)
        )
    else:
        db.execute(
            'INSERT INTO memberships (user_id, plan, started_at, expires_at) VALUES (?, ?, ?, ?)',
            (user_id, plan, now, expires_at)
        )
    db.commit()

    membership = db.execute('SELECT * FROM memberships WHERE user_id = ?', (user_id,)).fetchone()
    return jsonify({
        'message': f'Upgraded to {plan}',
        'membership': _membership_payload(membership),
        'price': valid_plans[plan],
    }), 200


@membership_bp.route('/plans', methods=['GET'])
def list_plans():
    """
    List all available membership plans and their pricing. Public endpoint.
    ---
    tags:
      - Membership
    responses:
      200:
        description: All available buyer and seller plans with features.
        schema:
          type: object
          properties:
            buyer_plans:
              type: array
              items:
                type: object
                properties:
                  plan:
                    type: string
                  price:
                    type: number
                  features:
                    type: array
                    items:
                      type: string
            seller_plans:
              type: array
              items:
                type: object
                properties:
                  plan:
                    type: string
                  price:
                    type: number
                  features:
                    type: array
                    items:
                      type: string
    """
    return jsonify({
        'buyer_plans': [
            {'plan': 'free', 'price': 0, 'features': ['Access to market', 'Track your orders']},
            {'plan': 'starter', 'price': 9.99, 'features': ['Everything in Free', 'Free Shipping']},
            {'plan': 'pro_farmer', 'price': 29.99, 'features': ['Everything in Starter', 'AI Assistant', 'Boost Listing Visibility']},
        ],
        'seller_plans': [
            {'plan': 'free', 'price': 0, 'features': ['Access to market', 'Track your orders']},
            {'plan': 'pro', 'price': 9.99, 'features': ['Everything in Free', 'Free Shipping', 'Alexa Integration']},
        ],
    }), 200
