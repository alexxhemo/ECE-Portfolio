"""
Run with:
python seed.py

Resets the database and inserts sample users and products.
"""

from run import app
from app import db, bcrypt
from app.models import (
    User, BuyerProfile, FarmProfile, Product, Membership
)

with app.app_context():
    db.drop_all()
    db.create_all()

    # -------------------------------------------------
    # Seller account
    # -------------------------------------------------
    seller = User(
        email='farmer@example.com',
        password_hash=bcrypt.generate_password_hash('password123').decode('utf-8'),
        role='seller',
    )
    db.session.add(seller)
    db.session.flush()

    db.session.add(FarmProfile(
        user_id=seller.id,
        farm_name='Cheshire Farms',
        biography='Family-owned farm with fresh local products.',
        pickup_address='123 Farm Rd, Fresno, CA',
        operating_hours='Mon-Sat 8am-5pm',
        zip_code='93701',
    ))
    db.session.add(Membership(user_id=seller.id, plan='pro'))

    # -------------------------------------------------
    # Buyer account
    # -------------------------------------------------
    buyer = User(
        email='buyer@example.com',
        password_hash=bcrypt.generate_password_hash('password123').decode('utf-8'),
        role='buyer',
    )
    db.session.add(buyer)
    db.session.flush()

    db.session.add(BuyerProfile(
        user_id=buyer.id,
        full_name='Jane Smith',
        phone='555-1234',
        zip_code='93701',
    ))
    db.session.add(Membership(user_id=buyer.id, plan='starter'))

    db.session.flush()

    # -------------------------------------------------
    # Sample products
    # -------------------------------------------------
    sample_products = [
        ('Cocktail Tomatoes', 'Fresh from the farm (4 pack)', 5.30, 30, 'Produce'),
        ('Carrots', 'Fresh from the farm (5 pack)', 2.48, 18, 'Vegetables'),
        ('Lacinato Kale', 'Fresh bunch of kale', 2.80, 7, 'Produce'),
        ('Sourdough Loaf', 'Baked fresh daily', 7.50, 10, 'Baked Goods'),
        ('Whole Milk (1 gal)', 'Pasture-raised milk', 4.99, 20, 'Dairy'),
    ]

    for name, description, price, quantity, category in sample_products:
        db.session.add(Product(
            seller_id=seller.id,
            name=name,
            description=description,
            price=price,
            quantity=quantity,
            category=category,
        ))

    db.session.commit()

    print('Seed complete.')
    print('Seller: farmer@example.com / password123')
    print('Buyer: buyer@example.com / password123')