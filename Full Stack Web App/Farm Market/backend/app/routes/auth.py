import json
import os
from urllib import parse, request as urlrequest

from flask import Blueprint, request, jsonify, redirect
from flask_jwt_extended import create_access_token, verify_jwt_in_request, get_jwt_identity
from werkzeug.security import generate_password_hash, check_password_hash

from app.db import get_db

auth_bp = Blueprint('auth', __name__)

def _frontend_url(path, query=None):
    base_url = os.getenv('FRONTEND_URL', 'http://localhost:3000').rstrip('/')

    if query:
        return f"{base_url}{path}?{parse.urlencode(query)}"

    return f"{base_url}{path}"


def _auth0_config():
    return {
        'domain': os.getenv('AUTH0_DOMAIN', '').strip(),
        'client_id': os.getenv('AUTH0_CLIENT_ID', '').strip(),
        'client_secret': os.getenv('AUTH0_CLIENT_SECRET', '').strip(),
        'audience': os.getenv('AUTH0_AUDIENCE', '').strip(),
        'callback_url': os.getenv(
            'AUTH0_CALLBACK_URL',
            'http://localhost:5000/api/auth/oauth/auth0/callback',
        ).strip(),
    }


def _create_default_profile(db, user_id, role, email):
    if role == 'buyer':
        db.execute(
            '''
            INSERT OR IGNORE INTO buyer_profiles (user_id, full_name, phone, zip_code)
            VALUES (?, ?, ?, ?)
            ''',
            (user_id, email.split('@')[0], '', ''),
        )
    else:
        db.execute(
            '''
            INSERT OR IGNORE INTO farm_profiles
            (user_id, farm_name, biography, pickup_address, operating_hours, zip_code)
            VALUES (?, ?, ?, ?, ?, ?)
            ''',
            (user_id, f"{email.split('@')[0]}'s Farm", '', '', '', ''),
        )

    db.execute(
        'INSERT OR IGNORE INTO memberships (user_id, plan, expires_at) VALUES (?, ?, ?)',
        (user_id, 'free', None),
    )


def _create_or_load_oauth_user(email, oauth_subject, role):
    db = get_db()

    user = db.execute(
        '''
        SELECT id FROM users
        WHERE oauth_provider = ? AND oauth_subject = ?
        ''',
        ('auth0', oauth_subject),
    ).fetchone()

    if user:
        return user['id']

    existing_user = db.execute(
        'SELECT id FROM users WHERE email = ?',
        (email,),
    ).fetchone()

    if existing_user:
        db.execute(
            '''
            UPDATE users
            SET oauth_provider = ?, oauth_subject = ?
            WHERE id = ?
            ''',
            ('auth0', oauth_subject, existing_user['id']),
        )
        db.commit()
        return existing_user['id']

    cursor = db.execute(
        '''
        INSERT INTO users (email, password_hash, role, oauth_provider, oauth_subject)
        VALUES (?, ?, ?, ?, ?)
        ''',
        (email, 'AUTH0_LOGIN_ONLY', role, 'auth0', oauth_subject),
    )

    user_id = cursor.lastrowid
    _create_default_profile(db, user_id, role, email)
    db.commit()

    return user_id


def _exchange_auth0_code(code, config):
    token_url = f"https://{config['domain']}/oauth/token"

    payload = {
        'grant_type': 'authorization_code',
        'client_id': config['client_id'],
        'client_secret': config['client_secret'],
        'code': code,
        'redirect_uri': config['callback_url'],
    }

    data = json.dumps(payload).encode('utf-8')

    req = urlrequest.Request(
        token_url,
        data=data,
        headers={'Content-Type': 'application/json'},
        method='POST',
    )

    with urlrequest.urlopen(req, timeout=10) as response:
        return json.loads(response.read().decode('utf-8'))


def _load_auth0_user(access_token, config):
    userinfo_url = f"https://{config['domain']}/userinfo"

    req = urlrequest.Request(
        userinfo_url,
        headers={'Authorization': f'Bearer {access_token}'},
        method='GET',
    )

    with urlrequest.urlopen(req, timeout=10) as response:
        return json.loads(response.read().decode('utf-8'))


def _membership_dict(row):
    if not row:
        return None
    return {
        'id': row['id'],
        'user_id': row['user_id'],
        'plan': row['plan'],
        'started_at': row['started_at'],
        'expires_at': row['expires_at'],
    }


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


def _load_user_bundle(user_id):
    db = get_db()
    user = db.execute(
        'SELECT id, email, role, created_at FROM users WHERE id = ?',
        (int(user_id),)
    ).fetchone()
    if not user:
        return None

    profile = None
    if user['role'] == 'buyer':
        profile = _buyer_profile_dict(db.execute(
            'SELECT * FROM buyer_profiles WHERE user_id = ?',
            (user['id'],)
        ).fetchone())
    else:
        profile = _seller_profile_dict(db.execute(
            'SELECT * FROM farm_profiles WHERE user_id = ?',
            (user['id'],)
        ).fetchone())

    membership = _membership_dict(db.execute(
        'SELECT * FROM memberships WHERE user_id = ?',
        (user['id'],)
    ).fetchone())

    return {
        'id': user['id'],
        'email': user['email'],
        'role': user['role'],
        'created_at': user['created_at'],
        'profile': profile,
        'membership': membership,
    }


@auth_bp.route('/oauth/auth0/start', methods=['GET'])
def auth0_start():
    role = request.args.get('role', 'buyer').strip().lower()

    if role not in ('buyer', 'seller'):
        return jsonify({'error': 'Role must be buyer or seller'}), 400

    config = _auth0_config()

    missing = [
        key for key in ('domain', 'client_id', 'callback_url')
        if not config[key]
    ]

    if missing:
        return jsonify({'error': f"Missing Auth0 config: {', '.join(missing)}"}), 500

    params = {
        'response_type': 'code',
        'client_id': config['client_id'],
        'redirect_uri': config['callback_url'],
        'scope': 'openid profile email',
        'state': role,
    }

    if config['audience']:
        params['audience'] = config['audience']

    auth_url = f"https://{config['domain']}/authorize?{parse.urlencode(params)}"

    return redirect(auth_url)


@auth_bp.route('/oauth/auth0/callback', methods=['GET'])
def auth0_callback():
    code = request.args.get('code', '')
    role = request.args.get('state', 'buyer').strip().lower()

    if role not in ('buyer', 'seller'):
        role = 'buyer'

    if not code:
        return redirect(_frontend_url('/oauth/callback', {'error': 'Missing Auth0 code'}))

    config = _auth0_config()

    missing = [
        key for key in ('domain', 'client_id', 'client_secret', 'callback_url')
        if not config[key]
    ]

    if missing:
        return redirect(_frontend_url(
            '/oauth/callback',
            {'error': f"Missing Auth0 config: {', '.join(missing)}"},
        ))

    try:
        token_response = _exchange_auth0_code(code, config)
        userinfo = _load_auth0_user(token_response['access_token'], config)
    except (KeyError, OSError, ValueError) as exc:
        return redirect(_frontend_url(
            '/oauth/callback',
            {'error': f'Auth0 login failed: {exc}'},
        ))

    email = str(userinfo.get('email', '')).strip().lower()
    oauth_subject = str(userinfo.get('sub', '')).strip()

    if not email or not oauth_subject:
        return redirect(_frontend_url(
            '/oauth/callback',
            {'error': 'Auth0 did not return email/sub'},
        ))

    user_id = _create_or_load_oauth_user(email, oauth_subject, role)
    token = create_access_token(identity=str(user_id))

    return redirect(_frontend_url('/oauth/callback', {'token': token}))

@auth_bp.route('/register', methods=['POST'])
def register():
    """
    Register a new user (buyer or seller).
    ---
    tags:
      - Auth
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          required:
            - email
            - password
            - role
          properties:
            email:
              type: string
              example: jane@example.com
            password:
              type: string
              example: secret123
            role:
              type: string
              enum: [buyer, seller]
              example: buyer
            full_name:
              type: string
              description: Buyer only
              example: Jane Smith
            phone:
              type: string
              description: Buyer only
              example: "555-1234"
            zip_code:
              type: string
              example: "06269"
            farm_name:
              type: string
              description: Seller only
              example: Cheshire Farms
            biography:
              type: string
              description: Seller only
              example: Family farm since 1980
            pickup_address:
              type: string
              description: Seller only
              example: 123 Farm Rd, Storrs CT
            operating_hours:
              type: string
              description: Seller only
              example: Mon-Sat 8am-5pm
    responses:
      201:
        description: Registered successfully. Returns JWT token and user bundle.
      400:
        description: Missing or invalid fields.
      409:
        description: Email already registered.
    """
    data = request.get_json() or {}

    required_fields = ['email', 'password', 'role']
    missing = [field for field in required_fields if not data.get(field)]
    if missing:
        return jsonify({'error': f"Missing fields: {', '.join(missing)}"}), 400

    email = str(data['email']).strip().lower()
    password = str(data['password'])
    role = str(data['role']).strip().lower()

    if role not in ('buyer', 'seller'):
        return jsonify({'error': 'Role must be buyer or seller'}), 400

    db = get_db()
    existing_user = db.execute(
        'SELECT id FROM users WHERE email = ?',
        (email,)
    ).fetchone()
    if existing_user:
        return jsonify({'error': 'Email already registered'}), 409

    password_hash = generate_password_hash(password)
    cursor = db.execute(
        'INSERT INTO users (email, password_hash, role) VALUES (?, ?, ?)',
        (email, password_hash, role)
    )
    user_id = cursor.lastrowid

    if role == 'buyer':
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
    else:
        db.execute(
            '''
            INSERT INTO farm_profiles (user_id, farm_name, biography, pickup_address, operating_hours, zip_code)
            VALUES (?, ?, ?, ?, ?, ?)
            ''',
            (
                user_id,
                data.get('farm_name', ''),
                data.get('biography', ''),
                data.get('pickup_address', ''),
                data.get('operating_hours', ''),
                data.get('zip_code', ''),
            )
        )

    db.execute(
        'INSERT INTO memberships (user_id, plan, expires_at) VALUES (?, ?, ?)',
        (user_id, 'free', None)
    )
    db.commit()

    token = create_access_token(identity=str(user_id))
    return jsonify({
        'message': 'Registered successfully',
        'token': token,
        'user': _load_user_bundle(user_id),
    }), 201


@auth_bp.route('/login', methods=['POST'])
def login():
    """
    Log in with email and password. Returns a JWT token.
    ---
    tags:
      - Auth
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          required:
            - email
            - password
          properties:
            email:
              type: string
              example: jane@example.com
            password:
              type: string
              example: secret123
    responses:
      200:
        description: Login successful. Returns JWT token and user bundle.
      400:
        description: Email and password are required.
      401:
        description: Invalid email or password.
    """
    data = request.get_json() or {}

    email = str(data.get('email', '')).strip().lower()
    password = data.get('password', '')

    if not email or not password:
        return jsonify({'error': 'Email and password required'}), 400

    db = get_db()
    user = db.execute(
        'SELECT id, email, password_hash, role, created_at FROM users WHERE email = ?',
        (email,)
    ).fetchone()

    if not user:
        return jsonify({'error': 'Invalid email or password'}), 401

    if user['password_hash'] == 'AUTH0_LOGIN_ONLY':
        return jsonify({'error': 'Please log in with Auth0'}), 401

    if not check_password_hash(user['password_hash'], password):
        return jsonify({'error': 'Invalid email or password'}), 401

    token = create_access_token(identity=str(user['id']))
    return jsonify({
        'message': 'Login successful',
        'token': token,
        'user': _load_user_bundle(user['id']),
    }), 200


@auth_bp.route('/me', methods=['GET'])
def me():
    """
    Get the currently authenticated user's profile and membership.
    ---
    tags:
      - Auth
    security:
      - Bearer: []
    responses:
      200:
        description: Current user data including role-specific profile and membership.
        schema:
          type: object
          properties:
            id:
              type: integer
            email:
              type: string
            role:
              type: string
              enum: [buyer, seller]
            created_at:
              type: string
            profile:
              type: object
            membership:
              type: object
      401:
        description: Missing or invalid JWT token.
      404:
        description: User not found.
    """
    verify_jwt_in_request()
    user_id = get_jwt_identity()

    bundle = _load_user_bundle(user_id)
    if not bundle:
        return jsonify({'error': 'User not found'}), 404

    return jsonify(bundle), 200
