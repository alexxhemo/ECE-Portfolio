import time


class CircuitBreaker:
    CLOSED = "closed"
    OPEN = "open"
    HALF_OPEN = "half_open"

    def __init__(self, failure_threshold=3, recovery_timeout=30):
        self.failure_threshold = failure_threshold
        self.recovery_timeout = recovery_timeout
        self._circuits = {}

    def _get(self, api_name):
        if api_name not in self._circuits:
            self._circuits[api_name] = {
                "state": self.CLOSED,
                "failures": 0,
                "last_failure_time": None,
            }
        return self._circuits[api_name]

    def allow_request(self, api_name):
        circuit = self._get(api_name)

        if circuit["state"] != self.OPEN:
            return True

        elapsed = time.time() - circuit["last_failure_time"]
        if elapsed >= self.recovery_timeout:
            circuit["state"] = self.HALF_OPEN
            return True

        return False

    def record_success(self, api_name):
        circuit = self._get(api_name)
        circuit["state"] = self.CLOSED
        circuit["failures"] = 0
        circuit["last_failure_time"] = None

    def record_failure(self, api_name):
        circuit = self._get(api_name)
        circuit["failures"] += 1
        circuit["last_failure_time"] = time.time()

        if circuit["state"] == self.HALF_OPEN or circuit["failures"] >= self.failure_threshold:
            circuit["state"] = self.OPEN

    def force_open(self, api_name):
        circuit = self._get(api_name)
        circuit["state"] = self.OPEN
        circuit["failures"] = self.failure_threshold
        circuit["last_failure_time"] = time.time()

    def reset(self, api_name=None):
        if api_name is None:
            self._circuits = {}
            return

        self._circuits[api_name] = {
            "state": self.CLOSED,
            "failures": 0,
            "last_failure_time": None,
        }

    def status(self, api_name=None):
        if api_name is not None:
            circuit = self._get(api_name)
            return {
                "api": api_name,
                "state": circuit["state"],
                "failures": circuit["failures"],
                "last_failure_time": circuit["last_failure_time"],
            }

        return self._circuits