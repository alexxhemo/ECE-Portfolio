
import sqlite3
import pytest



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


def create_product(client, seller_token, name='Apples', price=3.99, quantity=10):
    res = client.post(
        '/api/products',
        json={'name': name, 'price': price, 'quantity': quantity, 'category': 'Produce'},
        headers=auth_header(seller_token),
    )
    assert res.status_code == 201
    return res.get_json()['id']


#authentication 

def test_register_missing_email_rejected(client, app):
    res = client.post('/api/auth/register', json={'password': 'abc123', 'role': 'buyer'})
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM users') == 0


def test_register_invalid_role_rejected(client, app):
    res = client.post('/api/auth/register', json={
        'email': 'badrole@test.com', 'password': 'abc123', 'role': 'admin'
    })
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM users WHERE email = ?', ('badrole@test.com',)) == 0


def test_login_missing_fields_rejected(client):
    res = client.post('/api/auth/login', json={'email': 'nobody@test.com'})
    assert res.status_code == 400


def test_login_nonexistent_user_returns_401(client):
    res = client.post('/api/auth/login', json={'email': 'ghost@test.com', 'password': 'x'})
    assert res.status_code == 401


def test_me_without_token_returns_401(client):
    res = client.get('/api/auth/me')
    assert res.status_code == 401


#product

def test_create_product_zero_price_accepted_and_saved(client, app):
    """Price of exactly 0 is valid (free giveaway listing)."""
    register_user(client, 'sellerfree@test.com', 'seller')
    token = login_and_get_token(client, 'sellerfree@test.com')
    res = client.post('/api/products',
                      json={'name': 'Free Samples', 'price': 0, 'quantity': 5, 'category': 'Other'},
                      headers=auth_header(token))
    assert res.status_code == 201
    assert db_count(app, 'SELECT COUNT(*) FROM products WHERE name = ?', ('Free Samples',)) == 1


def test_create_product_zero_quantity_accepted_and_saved(client, app):
    """quantity=0 is valid (out-of-stock listing)."""
    register_user(client, 'sellerzero@test.com', 'seller')
    token = login_and_get_token(client, 'sellerzero@test.com')
    res = client.post('/api/products',
                      json={'name': 'Out of Stock Item', 'price': 5.0, 'quantity': 0, 'category': 'Dairy'},
                      headers=auth_header(token))
    assert res.status_code == 201
    assert db_count(app, 'SELECT COUNT(*) FROM products WHERE name = ?', ('Out of Stock Item',)) == 1


def test_create_product_negative_quantity_rejected_and_not_saved(client, app):
    register_user(client, 'sellernegqty@test.com', 'seller')
    token = login_and_get_token(client, 'sellernegqty@test.com')
    res = client.post('/api/products',
                      json={'name': 'Ghost Stock', 'price': 1.0, 'quantity': -3, 'category': 'Produce'},
                      headers=auth_header(token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM products WHERE name = ?', ('Ghost Stock',)) == 0


def test_create_product_missing_name_rejected(client, app):
    register_user(client, 'sellermissing@test.com', 'seller')
    token = login_and_get_token(client, 'sellermissing@test.com')
    res = client.post('/api/products',
                      json={'price': 2.0, 'quantity': 5},
                      headers=auth_header(token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM products') == 0


def test_get_nonexistent_product_returns_404(client):
    res = client.get('/api/products/99999')
    assert res.status_code == 404


def test_delete_product_with_reserved_inventory_rejected(client, app):
    register_user(client, 'sellerdelres@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerdelres@test.com')
    product_id = create_product(client, seller_token, name='Reserved Milk', quantity=5)

    register_user(client, 'buyerdelres@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerdelres@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 2},
                headers=auth_header(buyer_token))
    client.post('/api/orders', headers=auth_header(buyer_token))

    res = client.delete(f'/api/products/{product_id}', headers=auth_header(seller_token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM products WHERE id = ?', (product_id,)) == 1


def test_update_product_quantity_below_reserved_rejected(client, app):
    register_user(client, 'sellerupd@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerupd@test.com')
    product_id = create_product(client, seller_token, name='Corn', quantity=10)

    register_user(client, 'buyerupd@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerupd@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 5},
                headers=auth_header(buyer_token))
    client.post('/api/orders', headers=auth_header(buyer_token))

    # 5 are reserved; setting quantity to 2 should fail
    res = client.put(f'/api/products/{product_id}',
                     json={'quantity': 2},
                     headers=auth_header(seller_token))
    assert res.status_code == 400
    row = db_row(app, 'SELECT quantity FROM products WHERE id = ?', (product_id,))
    assert row['quantity'] == 10  # unchanged


def test_seller_cannot_update_another_sellers_product(client):
    register_user(client, 'seller_a@test.com', 'seller')
    register_user(client, 'seller_b@test.com', 'seller')
    token_a = login_and_get_token(client, 'seller_a@test.com')
    token_b = login_and_get_token(client, 'seller_b@test.com')
    product_id = create_product(client, token_a, name='Seller A Product')

    res = client.put(f'/api/products/{product_id}',
                     json={'price': 99.0},
                     headers=auth_header(token_b))
    assert res.status_code == 403


#cart

def test_add_to_cart_nonexistent_product_returns_404(client):
    register_user(client, 'buyernoitem@test.com', 'buyer')
    token = login_and_get_token(client, 'buyernoitem@test.com')
    res = client.post('/api/cart', json={'product_id': 99999, 'quantity': 1},
                      headers=auth_header(token))
    assert res.status_code == 404


def test_add_to_cart_accumulates_quantity(client, app):
    """Adding the same product twice sums quantities instead of duplicating rows."""
    register_user(client, 'selleracc@test.com', 'seller')
    seller_token = login_and_get_token(client, 'selleracc@test.com')
    product_id = create_product(client, seller_token, quantity=20)

    register_user(client, 'buyeracc@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyeracc@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 3},
                headers=auth_header(buyer_token))
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 4},
                headers=auth_header(buyer_token))

    assert db_count(app, 'SELECT COUNT(*) FROM cart_items') == 1
    row = db_row(app, 'SELECT quantity FROM cart_items')
    assert row['quantity'] == 7


def test_update_cart_item_to_zero_rejected(client, app):
    register_user(client, 'sellerucart@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerucart@test.com')
    product_id = create_product(client, seller_token, quantity=10)

    register_user(client, 'buyerucart@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerucart@test.com')
    add_res = client.post('/api/cart', json={'product_id': product_id, 'quantity': 2},
                          headers=auth_header(buyer_token))
    item_id = add_res.get_json()['id']

    res = client.put(f'/api/cart/{item_id}', json={'quantity': 0},
                     headers=auth_header(buyer_token))
    assert res.status_code == 400


def test_delete_nonexistent_cart_item_returns_404(client):
    register_user(client, 'buyerdel404@test.com', 'buyer')
    token = login_and_get_token(client, 'buyerdel404@test.com')
    res = client.delete('/api/cart/99999', headers=auth_header(token))
    assert res.status_code == 404


def test_clear_cart_removes_all_items(client, app):
    register_user(client, 'sellerclear@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerclear@test.com')
    p1 = create_product(client, seller_token, name='Item1', quantity=10)
    p2 = create_product(client, seller_token, name='Item2', quantity=10)

    register_user(client, 'buyerclear@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerclear@test.com')
    client.post('/api/cart', json={'product_id': p1, 'quantity': 1}, headers=auth_header(buyer_token))
    client.post('/api/cart', json={'product_id': p2, 'quantity': 1}, headers=auth_header(buyer_token))

    res = client.delete('/api/cart/clear', headers=auth_header(buyer_token))
    assert res.status_code == 200
    assert db_count(app, 'SELECT COUNT(*) FROM cart_items') == 0


#order

def test_cancel_pending_order_releases_inventory(client, app):
    register_user(client, 'sellercancel@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellercancel@test.com')
    product_id = create_product(client, seller_token, name='Berries', quantity=6)

    register_user(client, 'buyercancel@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyercancel@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 4},
                headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    assert db_row(app, 'SELECT reserved_qty FROM products WHERE id = ?', (product_id,))['reserved_qty'] == 4

    cancel_res = client.post(f'/api/orders/{order_id}/cancel', headers=auth_header(buyer_token))
    assert cancel_res.status_code == 200
    assert cancel_res.get_json()['status'] == 'cancelled'
    assert db_row(app, 'SELECT reserved_qty FROM products WHERE id = ?', (product_id,))['reserved_qty'] == 0


def test_cancel_completed_order_rejected(client, app):
    register_user(client, 'sellercanceldone@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellercanceldone@test.com')
    product_id = create_product(client, seller_token, name='Honey', quantity=5)

    register_user(client, 'buyercanceldone@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyercanceldone@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 1},
                headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    client.post(f'/api/orders/{order_id}/approve', headers=auth_header(seller_token))
    client.post(f'/api/orders/{order_id}/ready', headers=auth_header(seller_token))
    client.post(f'/api/orders/{order_id}/complete', headers=auth_header(buyer_token))

    res = client.post(f'/api/orders/{order_id}/cancel', headers=auth_header(buyer_token))
    assert res.status_code == 400
    assert db_row(app, 'SELECT status FROM orders WHERE id = ?', (order_id,))['status'] == 'completed'


def test_approve_already_confirmed_order_rejected(client, app):
    register_user(client, 'sellerdbl@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerdbl@test.com')
    product_id = create_product(client, seller_token, name='Tomatoes', quantity=5)

    register_user(client, 'buyerdbl@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerdbl@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 1},
                headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    client.post(f'/api/orders/{order_id}/approve', headers=auth_header(seller_token))
    res = client.post(f'/api/orders/{order_id}/approve', headers=auth_header(seller_token))
    assert res.status_code == 400


def test_complete_order_not_ready_rejected(client, app):
    register_user(client, 'sellernotready@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellernotready@test.com')
    product_id = create_product(client, seller_token, name='Eggs', quantity=12)

    register_user(client, 'buyernotready@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyernotready@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 1},
                headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    # Order is still pending — completing it should fail
    res = client.post(f'/api/orders/{order_id}/complete', headers=auth_header(buyer_token))
    assert res.status_code == 400


def test_buyer_cannot_approve_order(client):
    register_user(client, 'sellerbuyerapprove@test.com', 'seller')
    seller_token = login_and_get_token(client, 'sellerbuyerapprove@test.com')
    product_id = create_product(client, seller_token, name='Peaches', quantity=5)

    register_user(client, 'buyerbuyerapprove@test.com', 'buyer')
    buyer_token = login_and_get_token(client, 'buyerbuyerapprove@test.com')
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 1},
                headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']

    res = client.post(f'/api/orders/{order_id}/approve', headers=auth_header(buyer_token))
    assert res.status_code == 403


def test_approve_nonexistent_order_returns_404(client):
    register_user(client, 'seller404order@test.com', 'seller')
    token = login_and_get_token(client, 'seller404order@test.com')
    res = client.post('/api/orders/99999/approve', headers=auth_header(token))
    assert res.status_code == 404


#review

def _complete_order(client, seller_token, buyer_token, product_name, stock=5):
    product_id = create_product(client, seller_token, name=product_name, quantity=stock)
    client.post('/api/cart', json={'product_id': product_id, 'quantity': 1},
                headers=auth_header(buyer_token))
    order_res = client.post('/api/orders', headers=auth_header(buyer_token))
    order_id = order_res.get_json()['orders'][0]['id']
    client.post(f'/api/orders/{order_id}/approve', headers=auth_header(seller_token))
    client.post(f'/api/orders/{order_id}/ready', headers=auth_header(seller_token))
    client.post(f'/api/orders/{order_id}/complete', headers=auth_header(buyer_token))
    return order_id


def test_review_rating_below_1_rejected_and_not_saved(client, app):
    register_user(client, 'sellerrat0@test.com', 'seller')
    register_user(client, 'buyerrat0@test.com', 'buyer')
    seller_token = login_and_get_token(client, 'sellerrat0@test.com')
    buyer_token = login_and_get_token(client, 'buyerrat0@test.com')
    order_id = _complete_order(client, seller_token, buyer_token, 'Carrots')

    res = client.post('/api/reviews', json={'order_id': order_id, 'rating': 0},
                      headers=auth_header(buyer_token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM reviews WHERE order_id = ?', (order_id,)) == 0


def test_review_rating_above_5_rejected_and_not_saved(client, app):
    register_user(client, 'sellerrat6@test.com', 'seller')
    register_user(client, 'buyerrat6@test.com', 'buyer')
    seller_token = login_and_get_token(client, 'sellerrat6@test.com')
    buyer_token = login_and_get_token(client, 'buyerrat6@test.com')
    order_id = _complete_order(client, seller_token, buyer_token, 'Squash')

    res = client.post('/api/reviews', json={'order_id': order_id, 'rating': 6},
                      headers=auth_header(buyer_token))
    assert res.status_code == 400
    assert db_count(app, 'SELECT COUNT(*) FROM reviews WHERE order_id = ?', (order_id,)) == 0


def test_duplicate_review_rejected(client, app):
    register_user(client, 'sellerdup@test.com', 'seller')
    register_user(client, 'buyerdup@test.com', 'buyer')
    seller_token = login_and_get_token(client, 'sellerdup@test.com')
    buyer_token = login_and_get_token(client, 'buyerdup@test.com')
    order_id = _complete_order(client, seller_token, buyer_token, 'Plums')

    client.post('/api/reviews', json={'order_id': order_id, 'rating': 4},
                headers=auth_header(buyer_token))
    res = client.post('/api/reviews', json={'order_id': order_id, 'rating': 3},
                      headers=auth_header(buyer_token))
    assert res.status_code == 409
    assert db_count(app, 'SELECT COUNT(*) FROM reviews WHERE order_id = ?', (order_id,)) == 1


#membership

def test_seller_cannot_use_buyer_plan(client, app):
    register_user(client, 'sellerplan@test.com', 'seller')
    token = login_and_get_token(client, 'sellerplan@test.com')
    res = client.post('/api/membership/upgrade', json={'plan': 'pro_farmer'},
                      headers=auth_header(token))
    assert res.status_code == 400
    row = db_row(app, 'SELECT plan FROM memberships m JOIN users u ON u.id = m.user_id WHERE u.email = ?',
                 ('sellerplan@test.com',))
    assert row['plan'] == 'free'


def test_buyer_cannot_use_seller_plan(client, app):
    register_user(client, 'buyerplan@test.com', 'buyer')
    token = login_and_get_token(client, 'buyerplan@test.com')
    res = client.post('/api/membership/upgrade', json={'plan': 'pro'},
                      headers=auth_header(token))
    assert res.status_code == 400
    row = db_row(app, 'SELECT plan FROM memberships m JOIN users u ON u.id = m.user_id WHERE u.email = ?',
                 ('buyerplan@test.com',))
    assert row['plan'] == 'free'


def test_upgrade_to_paid_plan_sets_expires_at(client, app):
    register_user(client, 'memberpaid@test.com', 'buyer')
    token = login_and_get_token(client, 'memberpaid@test.com')
    client.post('/api/membership/upgrade', json={'plan': 'starter'}, headers=auth_header(token))
    row = db_row(app, 'SELECT expires_at FROM memberships m JOIN users u ON u.id = m.user_id WHERE u.email = ?',
                 ('memberpaid@test.com',))
    assert row['expires_at'] is not None


def test_downgrade_to_free_clears_expires_at(client, app):
    register_user(client, 'memberdown@test.com', 'buyer')
    token = login_and_get_token(client, 'memberdown@test.com')
    client.post('/api/membership/upgrade', json={'plan': 'starter'}, headers=auth_header(token))
    client.post('/api/membership/upgrade', json={'plan': 'free'}, headers=auth_header(token))
    row = db_row(app, 'SELECT expires_at FROM memberships m JOIN users u ON u.id = m.user_id WHERE u.email = ?',
                 ('memberdown@test.com',))
    assert row['expires_at'] is None


def test_membership_requires_auth(client):
    res = client.get('/api/membership')
    assert res.status_code == 401


#profile

def test_seller_cannot_access_buyer_profile_endpoint(client):
    register_user(client, 'sellerprof@test.com', 'seller')
    token = login_and_get_token(client, 'sellerprof@test.com')
    res = client.get('/api/profile/buyer', headers=auth_header(token))
    assert res.status_code == 403


def test_buyer_cannot_access_seller_profile_endpoint(client):
    register_user(client, 'buyerprof@test.com', 'buyer')
    token = login_and_get_token(client, 'buyerprof@test.com')
    res = client.get('/api/profile/seller', headers=auth_header(token))
    assert res.status_code == 403


def test_public_farm_profile_nonexistent_seller_returns_404(client):
    res = client.get('/api/profile/farm/99999')
    assert res.status_code == 404


def test_public_farm_profile_for_buyer_account_returns_404(client):
    """A buyer's user ID should not be accessible as a farm profile."""
    register_user(client, 'buyerfarm@test.com', 'buyer')
    token = login_and_get_token(client, 'buyerfarm@test.com')
    me_res = client.get('/api/auth/me', headers=auth_header(token))
    buyer_id = me_res.get_json()['id']
    res = client.get(f'/api/profile/farm/{buyer_id}')
    assert res.status_code == 404
