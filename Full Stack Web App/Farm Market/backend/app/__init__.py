import os
from flask import Flask, jsonify, request, g
from flask_bcrypt import Bcrypt
from flask_cors import CORS
from flask_jwt_extended import JWTManager
from flasgger import Swagger
from .circuit_breaker import CircuitBreaker

from .db import close_db
from .routes.auth import auth_bp
from .routes.profile import profile_bp
from .routes.products import products_bp
from .routes.cart import cart_bp
from .routes.orders import orders_bp
from .routes.reviews import reviews_bp
from .routes.membership import membership_bp

bcrypt = Bcrypt()
jwt = JWTManager()

def create_app(test_config=None):
    app = Flask(__name__, instance_relative_config=True)

    app.config.update(
        SECRET_KEY=os.getenv("SECRET_KEY", "dev-secret"),
        JWT_SECRET_KEY=os.getenv("JWT_SECRET_KEY", "jwt-secret"),
        DATABASE=os.getenv("DATABASE_PATH", os.path.join(app.instance_path, "farmmarket.db")),
        UPLOAD_FOLDER=os.path.join(app.root_path, "static", "uploads"),
        ALLOWED_EXTENSIONS={"png", "jpg", "jpeg", "gif", "webp"},
        MAX_CONTENT_LENGTH=8 * 1024 * 1024,
        SWAGGER={
            "title": "Farmer's Market Online API",
            "uiversion": 3,
            "securityDefinitions": {
                "Bearer": {
                    "type": "apiKey",
                    "name": "Authorization",
                    "in": "header",
                    "description": "JWT token. Format: **Bearer &lt;token&gt;**",
                }
            },
        },
    )

    if test_config:
        app.config.update(test_config)

    os.makedirs(app.instance_path, exist_ok=True)
    os.makedirs(app.config["UPLOAD_FOLDER"], exist_ok=True)

    CORS(app)
    bcrypt.init_app(app)
    jwt.init_app(app)

    app.circuit_breaker = CircuitBreaker(
        failure_threshold=app.config.get("CIRCUIT_BREAKER_FAILURE_THRESHOLD", 3),
        recovery_timeout=app.config.get("CIRCUIT_BREAKER_RECOVERY_TIMEOUT", 30),
    )

    @app.before_request
    def check_circuit_breaker():
        if not request.path.startswith("/api/"):
            return None

        if request.path.startswith("/api/circuit-breaker/"):
            return None

        api_name = f"{request.method} {request.url_rule.rule if request.url_rule else request.path}"
        g.circuit_breaker_api_name = api_name

        if not app.circuit_breaker.allow_request(api_name):
            return jsonify({
                "error": "Circuit breaker is open for this API. Try again later.",
                "api": api_name,
                "state": "open",
            }), 503

        return None

    @app.after_request
    def record_circuit_breaker_result(response):
        api_name = getattr(g, "circuit_breaker_api_name", None)

        if api_name:
            if response.status_code >= 500:
                app.circuit_breaker.record_failure(api_name)
            else:
                app.circuit_breaker.record_success(api_name)

        return response

    Swagger(
        app,
        config={
            "headers": [],
            "specs": [
                {
                    "endpoint": "apispec_1",
                    "route": "/apispec_1.json",
                    "rule_filter": lambda rule: True,
                    "model_filter": lambda tag: True,
                }
            ],
            "swagger_ui": True,
            "specs_route": "/apidocs/",
        },
    )

    app.teardown_appcontext(close_db)

    app.register_blueprint(auth_bp, url_prefix="/api/auth")
    app.register_blueprint(profile_bp, url_prefix="/api/profile")
    app.register_blueprint(products_bp, url_prefix="/api/products")
    app.register_blueprint(cart_bp, url_prefix="/api/cart")
    app.register_blueprint(orders_bp, url_prefix="/api/orders")
    app.register_blueprint(reviews_bp, url_prefix="/api/reviews")
    app.register_blueprint(membership_bp, url_prefix="/api/membership")

    @app.get("/api/circuit-breaker/status")
    def circuit_breaker_status():
        api_name = request.args.get("api")
        return jsonify(app.circuit_breaker.status(api_name)), 200

    @app.post("/api/circuit-breaker/reset")
    def circuit_breaker_reset():
        data = request.get_json(silent=True) or {}
        app.circuit_breaker.reset(data.get("api"))
        return jsonify({"message": "Circuit breaker reset"}), 200

    @app.post("/api/circuit-breaker/open")
    def circuit_breaker_open():
        data = request.get_json(silent=True) or {}
        api_name = data.get("api")

        if not api_name:
            return jsonify({"error": "api is required"}), 400

        app.circuit_breaker.force_open(api_name)
        return jsonify(app.circuit_breaker.status(api_name)), 200
    
    @app.get("/api/health")
    def health():
        """
        Health check endpoint.
        ---
        tags:
          - Health
        responses:
          200:
            description: Server is running.
            schema:
              type: object
              properties:
                status:
                  type: string
                  example: ok
        """
        return {"status": "ok"}, 200

    @app.errorhandler(404)
    def not_found(_error):
        return jsonify({"error": "Not found"}), 404

    @app.errorhandler(500)
    def server_error(_error):
        return jsonify({"error": "Internal server error"}), 500

    return app
