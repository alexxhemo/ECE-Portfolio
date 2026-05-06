# 🌾 Farmer's Market Online — React Frontend

A role-aware React SPA for a local farm marketplace. Buyers can browse products, manage a cart, place orders, and leave reviews. Sellers can manage listings, process incoming orders, and track their farm profile — all backed by a Flask REST API.

---

## Getting Started

### Prerequisites
- Node.js (v16+)
- The Flask backend running on `http://localhost:5000`

### Install & Run

```bash
cd frontend
npm install
npm start
```

Open **http://localhost:3000** in your browser. All `/api/*` requests are proxied to `http://localhost:5000` via the `proxy` field in `package.json`.

### Running with the Backend

In one terminal, start the Flask backend from the `backend/` folder. In a second terminal:

```bash
cd frontend
npm install
npm start
```

---

## Project Structure

```
src/
├── index.js                  # Entry point
├── App.jsx                   # Router + route protection
├── api.js                    # All API calls to Flask backend
├── styles/
│   └── global.css            # Design tokens + component styles
├── context/
│   └── AuthContext.jsx       # JWT auth state (login, register, logout)
├── components/
│   └── index.jsx             # Navbar, Sidebar, MacDonald AI widget,
│                             #   ProtectedRoute, StarRating, StatusBadge
└── pages/
    ├── Landing.jsx           # Public hero page with auth buttons
    ├── Auth.jsx              # BuyerRegister, SellerRegister, BuyerLogin, SellerLogin
    ├── Home.jsx              # Role-specific dashboard
    ├── Marketplace.jsx       # Browse products — search, category & zip filters
    ├── Cart.jsx              # Cart with quantity controls + checkout
    ├── Profile.jsx           # Buyer contact profile / Seller farm profile
    ├── MyOrders.jsx          # Buyer orders — cancel, complete, review modal
    ├── IncomingOrders.jsx    # Seller order management — approve, reject, ready, complete
    ├── MyListings.jsx        # Seller product CRUD — inline edit + stock filters
    ├── Reviews.jsx           # Buyer: reviews written / Seller: reviews received + avg rating
    └── Membership.jsx        # Plan cards with upgrade flow
```

---

## Routes

| Path | Access | Page |
|---|---|---|
| `/` | Public | Landing page |
| `/buyer/register` | Public | Buyer registration |
| `/buyer/login` | Public | Buyer login |
| `/seller/register` | Public | Seller registration |
| `/seller/login` | Public | Seller login |
| `/home` | Any authenticated user | Role-specific dashboard |
| `/profile` | Any authenticated user | Buyer or Seller profile |
| `/reviews` | Any authenticated user | Reviews (role-aware) |
| `/membership` | Any authenticated user | Membership plan management |
| `/marketplace` | Buyer only | Browse & filter products |
| `/cart` | Buyer only | Shopping cart + checkout |
| `/orders` | Buyer only | My orders + status filters |
| `/listings` | Seller only | My product listings |
| `/listings/new` | Seller only | Create new listing |
| `/incoming` | Seller only | Incoming orders dashboard |

---

## Auth Flow

1. JWT token is stored in `localStorage` under the key `fm_token`
2. `AuthContext` hydrates on page load via `GET /api/auth/me`
3. `ProtectedRoute` checks authentication and role before rendering any page
4. Role mismatch → redirected to `/home`
5. Not authenticated → redirected to `/`

---

## Design System

Styles live in `src/styles/global.css` and use CSS custom properties throughout.

**Tokens**

| Category | Variables |
|---|---|
| Colors | `--green-dark`, `--green-mid`, `--green-light`, `--navy`, `--cream` |
| Fonts | Outfit (display/headings), Poppins (body) |
| Border radius | `--radius-sm` (6px) → `--radius-full` (9999px) |
| Shadows | `--shadow-sm`, `--shadow-md`, `--shadow-lg` |

**Reusable classes:** `.btn` · `.card` · `.chip` · `.badge` · `.form-input` · `.product-card` · `.order-card` · `.plan-card`

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
