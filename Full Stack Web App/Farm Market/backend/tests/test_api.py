import sqlite3


def register_user(client, email, role, password='password123'):
    payload = {
        'email': email,
        'password': password,
        'role': role,
        'zip_code': '06269',
    }
    if role == 'buyer':
        payload['full_name'] = 'Test Buyer'
        payload['phone'] = '5551112222'
    else:
        payload['farm_name'] = 'Test Farm'
        payload['biography'] = 'Family farm'
        payload['pickup_address'] = '123 Farm Rd'
        payload['operating_hours'] = 'Mon-Fri 9-5'
    return client.post('/api/auth/register', json=payload)


def login_and_get_token(client, email, password='password123'):
    res = client.post('/api/auth/login', json={'email': email, 'password': password})
    assert res.status_code == 200
    return res.get_json()['token']


def auth_header(token):
    return {'Authorization': f'Bearer {token}'}


def db_count(app, query, params=()):
    conn = sqlite3.connect(app.config['DATABASE'])
    try:
        return conn.execute(query, params).fetchone()[0]
    finally:
        conn.close()


def db_row(app, query, params=()):
    conn = sqlite3.connect(app.config['DATABASE'])
    try:
        conn.row_factory = sqlite3.Row
        row = conn.execute(query, params).fetchone()
        return dict(row) if row else None
    finally:
        conn.close()


def create_product_for_seller(client, seller_token, name='Apples', price=3.99, quantity=10):
    return client.post(
        '/api/products',
        json={'name': name, 'price': price, 'quantity': quantity, 'category': 'Produce'},
        headers=auth_header(seller_token),
    )


def test_register_buyer_creates_user_profile_and_membership(client, app):
    res = register_user(client, 'buyer@test.com', 'buyer')
    body = res.get_json()
    assert res.status_code == 201
    assert 'token' in body
    assert body['user']['role'] == 'buyer'

    assert db_count(app, 'SELECT COUNT(*) FROM users WHERE email = ?', ('buyer@test.com',)) == 1
    user = db_row(app, 'SELECT id FROM users WHERE email = ?', ('buyer@test.com',))
    assert db_count(app, 'SELECT COUNT(*) FROM buyer_profiles WHERE user_id = ?', (user['id'],)) == 1
    assert db_count(app, 'SELECT COUNT(*) FROM memberships WHERE user_id = ?', (user['id'],)) == 1


def test_register_duplicate_email_keeps_single_user_row(client, app):
    assert register_user(client, 'dup@test.com', 'buyer').status_code == 201
    res = register_user(client, 'dup@test.com', 'buyer')
    assert res.status_code == 409
    assert db_count(app, 'SELECT COUNT(*) FROM users WHERE email = ?', ('dup@test.com',)) == 1


def test_login_success_returns_token(client):
    register_user(client, 'login@test.com', 'buyer')
    res = client.post('/api/auth/login', json={'email': 'login@test.com', 'password': 'password123'})
    assert res.status_code == 200
    assert 'token' in res.get_json()


def test_login_wrong_password_does_not_change_db(client, app):
    register_user(client, 'wrong@test.com', 'buyer')
    before = db_count(app, 'SELECT COUNT(*) FROM users', ())
    res = client.post('/api/auth/login', json={'email': 'wrong@test.com', 'password': 'wrongpassword'})
    assert res.status_code == 401
    after = db_count(app, 'SELECT COUNT(*) FROM users', ())
    assert before == after


def test_get_me_returns_logged_in_user(client):
    register_user(client, 'me@test.com', 'buyer')
    token = login_and_get_token(client, 'me@test.com')
    res = client.get('/api/auth/me', headers=auth_header(token))
    assert res.status_code == 200
    assert res.get_json()['email'] == 'me@test.com'


def test_list_products_empty_ok(client):
    res = client.get('/api/products')
    assert res.status_code == 200
    assert res.get_json() == []


def test_create_product_success_and_verify_db(client, app):
    register_user(client, 'seller@test.com', 'seller')
    seller_token = login_and_get_token(client, 'seller@test.com')
    res = create_product_for_seller(client, seller_token, quantity=50)
    assert res.status_code == 201
    assert db_count(app, 'SELECT COUNT(*) FROM products WHERE name = ?', ('Apples',)) == 1


def test_create_product_negative_price_rejected_and_not_saved(client, app):
    register_user(client, 'sellerneg@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerneg@test.com')
    res = client.post(
        '/api/products',
        json={'name': 'Bad Apples', 'price': -1, 'quantity': 5, 'category': 'Produce'},
        headers=auth_header(seller_token),
    )
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM products WHERE name = ?', ('Bad Apples',)) == 0


def test_create_product_buyer_forbidden(client):
    register_user(client, 'buyer2@test.com', 'buyer')
    token = login_and_get_token(client, 'buyer2@test.com')
    res = client.post(
        '/api/products',
        json={'name': 'Apples', 'price': 3.99, 'quantity': 10},
        headers=auth_header(token),
    )
    assert res.status_code == 403


def test_get_buyer_profile(client):
    register_user(client, 'profile@test.com', 'buyer')
    token = login_and_get_token(client, 'profile@test.com')
    res = client.get('/api/profile/buyer', headers=auth_header(token))
    assert res.status_code == 200
    assert res.get_json()['zip_code'] == '06269'


def test_cart_requires_auth(client):
    res = client.get('/api/cart')
    assert res.status_code == 401


def test_add_to_cart_zero_quantity_rejected_and_not_saved(client, app):
    register_user(client, 'sellercart@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellercart@test.com')
    product_res = create_product_for_seller(client, seller_token, quantity=10)
    product_id = product_res.get_json()['id']

    register_user(client, 'buyercart@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyercart@test.com')
    res = client.post(
        '/api/cart',
        json={'product_id': product_id, 'quantity': 0},
        headers=auth_header(buyer_token),
    )
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM cart_items', ()) == 0


def test_add_to_cart_too_many_rejected_and_not_saved(client, app):
    register_user(client, 'sellerstock@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerstock@test.com')
    product_res = create_product_for_seller(client, seller_token, quantity=3)
    product_id = product_res.get_json()['id']

    register_user(client, 'buyerstock@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerstock@test.com')
    res = client.post(
        '/api/cart',
        json={'product_id': product_id, 'quantity': 5},
        headers=auth_header(buyer_token),
    )
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM cart_items', ()) == 0


def test_place_order_empty_cart(client, app):
    register_user(client, 'orderer@test.com', 'buyer')
    token = login_and_get_token(client, 'orderer@test.com')
    res = client.post('/api/orders', headers=auth_header(token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM orders', ()) == 0


def test_full_order_flow_and_review_with_db_checks(client, app):
    register_user(client, 'sellerflow@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerflow@test.com')
    product_res = create_product_for_seller(client, seller_token, name='Peaches', quantity=8)
    product_id = product_res.get_json()['id']

    register_user(client, 'buyerflow@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerflow@test.com')

    add_res = client.post('/api/cart', json={'product_id': product_id, 'quantity': 2}, headers=auth_header(buyer_token))
    assert add_res.status_code in (200, 201)

    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    assert order_res.status_code == 201
    order_id = order_res.get_json()['orders'][0]['id']

    product_after_place = db_row(app, 'SELECT quantity, reserved_qty FROM products WHERE id = ?', (product_id,))
    assert product_after_place['quantity'] == 8
    assert product_after_place['reserved_qty'] == 2

    approve_res = client.post(f'/api/orders/{order_id}/approve', headers=auth_header(seller_token))
    assert approve_res.status_code == 200
    product_after_approve = db_row(app, 'SELECT quantity, reserved_qty FROM products WHERE id = ?', (product_id,))
    assert product_after_approve['quantity'] == 6
    assert product_after_approve['reserved_qty'] == 0

    ready_res = client.post(f'/api/orders/{order_id}/ready', headers=auth_header(seller_token))
    assert ready_res.status_code == 200
    complete_res = client.post(f'/api/orders/{order_id}/complete', headers=auth_header(buyer_token))
    assert complete_res.status_code == 200

    review_res = client.post(
        '/api/reviews',
        json={'order_id': order_id, 'rating': 5, 'comment': 'Fresh and delicious'},
        headers=auth_header(buyer_token),
    )
    assert review_res.status_code == 201
    assert db_count(app, 'SELECT COUNT(*) FROM reviews WHERE order_id = ?', (order_id,)) == 1


def test_reject_order_releases_reserved_inventory(client, app):
    register_user(client, 'sellerreject@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerreject@test.com')
    product_res = create_product_for_seller(client, seller_token, name='Milk', quantity=7)
    product_id = product_res.get_json()['id']

    register_user(client, 'buyerreject@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerreject@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 3}, headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    reserved_before = db_row(app, 'SELECT reserved_qty FROM products WHERE id = ?', (product_id,))
    assert reserved_before['reserved_qty'] == 3

    reject_res = client.post(f'/api/orders/{order_id}/reject', headers=auth_header(seller_token))
    assert reject_res.status_code == 200
    reserved_after = db_row(app, 'SELECT reserved_qty FROM products WHERE id = ?', (product_id,))
    assert reserved_after['reserved_qty'] == 0


def test_review_on_nonexistent_order(client):
    register_user(client, 'reviewer@test.com', 'buyer')
    token = login_and_get_token(client, 'reviewer@test.com')
    res = client.post('/api/reviews', json={'order_id': 9999, 'rating': 5}, headers=auth_header(token))
    assert res.status_code == 404


def test_review_on_incomplete_order_rejected_and_not_saved(client, app):
    register_user(client, 'sellerincomplete@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerincomplete@test.com')
    product_res = create_product_for_seller(client, seller_token, name='Bread', quantity=4)
    product_id = product_res.get_json()['id']

    register_user(client, 'buyerincomplete@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerincomplete@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 1}, headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    res = client.post('/api/reviews', json={'order_id': order_id, 'rating': 5}, headers=auth_header(buyer_token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM reviews WHERE order_id = ?', (order_id,)) == 0


def test_get_membership(client):
    register_user(client, 'member@test.com', 'buyer')
    token = login_and_get_token(client, 'member@test.com')
    res = client.get('/api/membership', headers=auth_header(token))
    assert res.status_code == 200
    assert res.get_json()['plan'] == 'free'


def test_invalid_membership_plan_rejected_and_db_unchanged(client, app):
    register_user(client, 'memberbad@test.com', 'buyer')
    token = login_and_get_token(client, 'memberbad@test.com')
    before = db_row(app, 'SELECT plan FROM memberships m JOIN users u ON u.id = m.user_id WHERE u.email = ?', ('memberbad@test.com',))
    res = client.post('/api/membership/upgrade', json={'plan': 'ultra'}, headers=auth_header(token))
    assert res.status_code == 400
    after = db_row(app, 'SELECT plan FROM memberships m JOIN users u ON u.id = m.user_id WHERE u.email = ?', ('memberbad@test.com',))
    assert before['plan'] == after['plan'] == 'free'


def test_list_plans_public(client):
    res = client.get('/api/membership/plans')
    assert res.status_code == 200
    body = res.get_json()
    assert 'buyer_plans' in body
    assert 'seller_plans' in body


def test_seller_reviews_public(client):
    res = client.get('/api/reviews/seller/1')
    assert res.status_code == 200
    assert 'reviews' in res.get_json()


def test_incoming_orders_requires_seller(client):
    register_user(client, 'buyerx@test.com', 'buyer')
    token = login_and_get_token(client, 'buyerx@test.com')
    res = client.get('/api/orders/incoming', headers=auth_header(token))
    assert res.status_code == 403