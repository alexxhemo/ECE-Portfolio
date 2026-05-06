from app.circuit_breaker import CircuitBreaker


def test_circuit_breaker_opens_and_resets_for_each_api_call():
    breaker = CircuitBreaker(failure_threshold=2, recovery_timeout=60)

    register_api = "POST /api/auth/register"
    products_api = "GET /api/products"

    assert breaker.allow_request(register_api)
    assert breaker.allow_request(products_api)

    breaker.record_failure(register_api)
    assert breaker.allow_request(register_api)

    breaker.record_failure(register_api)
    assert not breaker.allow_request(register_api)

    assert breaker.allow_request(products_api)

    breaker.reset(register_api)
    assert breaker.allow_request(register_api)
    assert breaker.status(register_api)["state"] == CircuitBreaker.CLOSED


def test_circuit_breaker_half_open_success_resets_after_timeout():
    breaker = CircuitBreaker(failure_threshold=1, recovery_timeout=0)
    api_name = "GET /api/cart"

    breaker.record_failure(api_name)
    assert breaker.status(api_name)["state"] == CircuitBreaker.OPEN

    assert breaker.allow_request(api_name)
    assert breaker.status(api_name)["state"] == CircuitBreaker.HALF_OPEN

    breaker.record_success(api_name)
    assert breaker.status(api_name)["state"] == CircuitBreaker.CLOSED
    assert breaker.status(api_name)["failures"] == 0


def test_circuit_breaker_middleware_blocks_and_resets_api(client, app):
    api_name = "GET /api/health"

    app.circuit_breaker.force_open(api_name)

    blocked = client.get("/api/health")
    assert blocked.status_code == 503

    reset = client.post("/api/circuit-breaker/reset", json={"api": api_name})
    assert reset.status_code == 200

    allowed = client.get("/api/health")
    assert allowed.status_code == 200


def test_circuit_breaker_is_per_api(client, app):
    app.circuit_breaker.force_open("GET /api/products")

    blocked = client.get("/api/products")
    assert blocked.status_code == 503

    other = client.get("/api/health")
    assert other.status_code == 200