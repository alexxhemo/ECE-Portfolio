from functools import wraps
import os
import uuid

from flask import jsonify, current_app
from flask_jwt_extended import verify_jwt_in_request, get_jwt_identity
from werkzeug.utils import secure_filename

from app.db import get_db

def _get_current_user():
    user_id = get_jwt_identity()
    db = get_db()
    return db.execute(
        "SELECT id, email, role FROM users WHERE id = ?",
        (int(user_id),)
    ).fetchone()

def login_required(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
        verify_jwt_in_request()
        user = _get_current_user()
        if not user:
            return jsonify({"error": "Authentication required"}), 401
        return fn(*args, **kwargs)
    return wrapper

def buyer_required(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
        verify_jwt_in_request()
        user = _get_current_user()
        if not user:
            return jsonify({"error": "Authentication required"}), 401
        if user["role"] != "buyer":
            return jsonify({"error": "Buyers only"}), 403
        return fn(*args, **kwargs)
    return wrapper

def seller_required(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
        verify_jwt_in_request()
        user = _get_current_user()
        if not user:
            return jsonify({"error": "Authentication required"}), 401
        if user["role"] != "seller":
            return jsonify({"error": "Sellers only"}), 403
        return fn(*args, **kwargs)
    return wrapper

def allowed_file(filename):
    return (
        "." in filename and
        filename.rsplit(".", 1)[1].lower() in current_app.config["ALLOWED_EXTENSIONS"]
    )

def save_upload(file):
    if not file or not file.filename:
        return None

    if not allowed_file(file.filename):
        return None

    original_name = secure_filename(file.filename)
    extension = original_name.rsplit(".", 1)[1].lower()
    filename = f"{uuid.uuid4().hex}.{extension}"
    filepath = os.path.join(current_app.config["UPLOAD_FOLDER"], filename)
    file.save(filepath)
    return filename