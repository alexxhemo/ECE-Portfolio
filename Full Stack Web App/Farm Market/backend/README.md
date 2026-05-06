# 🌾 Farmer's Market Online — Flask Backend

A RESTful Flask API for a local farm marketplace. Handles authentication, product listings, cart management, order lifecycle, reviews, and membership plans — backed by SQLite with JWT-protected routes.

---

## Getting Started

### Run Directly

```bash
cd backend
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
python init_db.py
python run.py
```

> Run `python init_db.py` once before starting the server to create the SQLite schema.

### Run with Docker

```bash
docker build -t farmmarket-Team04-backend ./backend
docker run --rm -p 5000:5000 farmmarket-Team04-backend
```

**Backend:** http://localhost:5000  
**Swagger UI:** http://localhost:5000/apidocs

---

## Project Structure

```
backend/
├── run.py                  # Entry point
├── init_db.py              # Initializes SQLite schema
├── requirements.txt
└── app/
    ├── db.py               # SQLite connection helper
    ├── utils.py            # Auth decorators + file upload helpers
    ├── static/uploads/     # Uploaded product images
    └── routes/
        ├── auth.py         # /api/auth
        ├── profile.py      # /api/profile
        ├── products.py     # /api/products
        ├── cart.py         # /api/cart
        ├── orders.py       # /api/orders
        ├── reviews.py      # /api/reviews
        └── membership.py   # /api/membership
```

---

## Database Schema

| Table | Key Fields |
|---|---|
| `users` | `id`, `email`, `password_hash`, `role` (buyer/seller) |
| `buyer_profiles` | `user_id`, `full_name`, `phone`, `zip_code` |
| `farm_profiles` | `user_id`, `farm_name`, `biography`, `pickup_address`, `operating_hours`, `zip_code` |
| `products` | `seller_id`, `name`, `price`, `quantity`, `reserved_qty`, `image_path`, `category` |
| `cart_items` | `buyer_id`, `product_id`, `quantity` |
| `orders` | `buyer_id`, `seller_id`, `status` |
| `order_items` | `order_id`, `product_id`, `quantity`, `price_at_purchase` |
| `reviews` | `buyer_id`, `seller_id`, `order_id`, `rating`, `comment` |
| `memberships` | `user_id`, `plan`, `expires_at` |

---

## Order Status Flow

```
pending → confirmed → ready_for_pickup → completed
        ↘ rejected          (seller action)
pending / confirmed → cancelled    (buyer action)
```

---

## API Reference

All protected routes require the header:
```
Authorization: Bearer <jwt_token>
```

---

### 🔐 Auth — `/api/auth`

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| `POST` | `/register` | No | Register a buyer or seller |
| `POST` | `/login` | No | Login, returns JWT |
| `GET` | `/me` | Yes | Get current user + profile |

**Register buyer**
```json
{
  "email": "jane@example.com",
  "password": "secret",
  "role": "buyer",
  "full_name": "Jane Smith",
  "phone": "555-1234",
  "zip_code": "93701"
}
```

**Register seller**
```json
{
  "email": "farm@example.com",
  "password": "secret",
  "role": "seller",
  "farm_name": "Cheshire Farms",
  "biography": "Family farm since 1980",
  "pickup_address": "123 Farm Rd, Fresno CA",
  "operating_hours": "Mon-Sat 8am-5pm",
  "zip_code": "93701"
}
```

---

### 👤 Profile — `/api/profile`

| Method | Endpoint | Auth | Role | Description |
|---|---|---|---|---|
| `GET` | `/buyer` | Yes | Buyer | Get my buyer profile |
| `PUT` | `/buyer` | Yes | Buyer | Update my buyer profile |
| `GET` | `/seller` | Yes | Seller | Get my farm profile |
| `PUT` | `/seller` | Yes | Seller | Update my farm profile |
| `GET` | `/farm/<seller_id>` | No | Any | Public farm page |

---

### 🛒 Products — `/api/products`

| Method | Endpoint | Auth | Role | Description |
|---|---|---|---|---|
| `GET` | `/` | No | Any | List / search marketplace products |
| `GET` | `/<id>` | No | Any | Product detail |
| `POST` | `/` | Yes | Seller | Create listing (`multipart/form-data`) |
| `PUT` | `/<id>` | Yes | Seller | Edit listing |
| `DELETE` | `/<id>` | Yes | Seller | Delete listing |
| `GET` | `/mine` | Yes | Seller | My listings |

**Query params for `GET /api/products`**

| Param | Description |
|---|---|
| `category` | `Produce`, `Dairy`, `Meat`, `Baked Goods`, `Fruit`, `Vegetables`, `Other` |
| `zip_code` | Filter by farm zip code |
| `search` | Keyword search on product name |
| `seller_id` | Filter by seller |

**Create listing fields** (`multipart/form-data`): `name`, `description`, `price`, `quantity`, `category`, `image` (file)

---

### 🛍️ Cart — `/api/cart`

| Method | Endpoint | Auth | Role | Description |
|---|---|---|---|---|
| `GET` | `/` | Yes | Buyer | View cart |
| `POST` | `/` | Yes | Buyer | Add item |
| `PUT` | `/<item_id>` | Yes | Buyer | Update quantity |
| `DELETE` | `/<item_id>` | Yes | Buyer | Remove item |
| `DELETE` | `/clear` | Yes | Buyer | Clear entire cart |

---

### 📦 Orders — `/api/orders`

| Method | Endpoint | Auth | Role | Description |
|---|---|---|---|---|
| `POST` | `/` | Yes | Buyer | Place order from cart |
| `GET` | `/mine` | Yes | Buyer | My orders (filter: `?status=`) |
| `POST` | `/<id>/cancel` | Yes | Buyer | Cancel a pending or confirmed order |
| `GET` | `/incoming` | Yes | Seller | Incoming orders (filter: `?status=`) |
| `POST` | `/<id>/approve` | Yes | Seller | Approve → `confirmed` |
| `POST` | `/<id>/reject` | Yes | Seller | Reject → releases reserved inventory |
| `POST` | `/<id>/ready` | Yes | Seller | Mark ready for pickup |
| `POST` | `/<id>/complete` | Yes | Buyer/Seller | Mark completed |
| `GET` | `/<id>` | Yes | Buyer/Seller | Order detail |

**Valid status values:** `pending`, `confirmed`, `ready_for_pickup`, `completed`, `rejected`, `cancelled`

---

### ⭐ Reviews — `/api/reviews`

| Method | Endpoint | Auth | Role | Description |
|---|---|---|---|---|
| `POST` | `/` | Yes | Buyer | Leave a review (completed orders only) |
| `GET` | `/seller/<seller_id>` | No | Any | All reviews for a seller |
| `GET` | `/mine` | Yes | Buyer | My submitted reviews |

```json
{
  "order_id": 1,
  "rating": 5,
  "comment": "Fresh and delicious!"
}
```

---

### 🎖️ Membership — `/api/membership`

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| `GET` | `/` | Yes | Get my current plan |
| `POST` | `/upgrade` | Yes | Upgrade plan |
| `GET` | `/plans` | No | List all plans + pricing |

| Role | Plans |
|---|---|
| Buyer | `free`, `starter` ($9.99/mo), `pro_farmer` ($29.99/mo) |
| Seller | `free`, `pro` ($9.99/mo) |

---

## Role Isolation

| Feature | Buyer | Seller |
|---|---|---|
| Browse marketplace | ✅ | ❌ |
| Read public products API | ✅ | ✅ |
| Add to cart / checkout | ✅ | ❌ |
| View own orders | ✅ | ❌ |
| Cancel order | ✅ | ❌ |
| Leave reviews | ✅ | ❌ |
| Create / edit / delete listings | ❌ | ✅ |
| Approve / reject / mark ready | ❌ | ✅ |
| View incoming orders | ❌ | ✅ |
| Edit farm profile | ❌ | ✅ |

---

## Frontend Integration

Protect routes by role using React Router v6:

```jsx
<Route path="/marketplace" element={<ProtectedRoute role="buyer"><Marketplace /></ProtectedRoute>} />
<Route path="/cart"        element={<ProtectedRoute role="buyer"><Cart /></ProtectedRoute>} />
<Route path="/orders"      element={<ProtectedRoute role="buyer"><MyOrders /></ProtectedRoute>} />

<Route path="/listings"    element={<ProtectedRoute role="seller"><MyListings /></ProtectedRoute>} />
<Route path="/incoming"    element={<ProtectedRoute role="seller"><IncomingOrders /></ProtectedRoute>} />
```

Store the JWT in `localStorage` and attach it to every protected request:

```js
headers: { Authorization: `Bearer ${token}` }
```

---

## Suggested Demo Flow

> Useful for TA walkthroughs or manual testing of the full order lifecycle.

1. Register a **seller** account
2. Fill in the seller profile (farm name, address, hours)
3. Create a product listing
4. Register a **buyer** account
5. Fill in the buyer profile
6. Browse the **Marketplace** and add an item to cart
7. Open **Cart** and place the order
8. Log back in as the **seller** and open **Incoming Orders**
9. Approve the order
10. Mark the order as ready for pickup
11. Log back in as the **buyer** and open **My Orders**
12. Mark the order as completed
13. Leave a review for the seller
14. Open **Reviews** to verify it appears
