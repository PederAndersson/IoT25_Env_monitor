import logging

import pytest

from src.iot_receiver import mqtt_receiver


class FakeMqttClient:
    def __init__(self, callback_api_version):
        self.callback_api_version = callback_api_version
        self.credentials = None
        self.ca_cert_path = None
        self.connection = None
        self.loop_started = False
        self.on_connect = None
        self.on_message = None
        self.on_disconnect = None

    def username_pw_set(self, username, password):
        self.credentials = (username, password)

    def tls_set(self, ca_certs):
        self.ca_cert_path = ca_certs

    def connect(self, broker, port, keepalive):
        self.connection = (broker, port, keepalive)

    def loop_forever(self):
        self.loop_started = True


class FakeSubscriber:
    def __init__(self):
        self.topics = []

    def subscribe(self, topic):
        self.topics.append(topic)


@pytest.mark.parametrize(
    ("broker", "port"),
    [(None, None), ("broker.example.invalid", None), (None, "8883")],
)
def test_main_rejects_missing_broker_or_port(monkeypatch, broker, port):
    if broker is None:
        monkeypatch.delenv("MQTT_BROKER", raising=False)
    else:
        monkeypatch.setenv("MQTT_BROKER", broker)
    if port is None:
        monkeypatch.delenv("MQTT_PORT", raising=False)
    else:
        monkeypatch.setenv("MQTT_PORT", port)

    assert mqtt_receiver.main() == 1


@pytest.mark.parametrize("port", ["not-a-number", "0", "65536"])
def test_main_rejects_invalid_port(monkeypatch, port):
    monkeypatch.setenv("MQTT_BROKER", "broker.example.invalid")
    monkeypatch.setenv("MQTT_PORT", port)

    assert mqtt_receiver.main() == 1


def test_main_configures_client_without_network_or_database(
    monkeypatch, tmp_path
):
    created_clients = []

    def create_client(callback_api_version):
        client = FakeMqttClient(callback_api_version)
        created_clients.append(client)
        return client

    monkeypatch.setenv("MQTT_BROKER", "broker.example.invalid")
    monkeypatch.setenv("MQTT_PORT", "8883")
    monkeypatch.setenv("MQTT_USERNAME", "test-user")
    monkeypatch.setenv("MQTT_PASSWORD", "test-password")
    monkeypatch.setenv("MQTT_CA_PATH", str(tmp_path / "ca.crt"))
    monkeypatch.setattr(mqtt_receiver, "initialize_database", lambda: None)
    monkeypatch.setattr(mqtt_receiver.mqtt, "Client", create_client)

    result = mqtt_receiver.main()

    assert result is None
    assert len(created_clients) == 1
    client = created_clients[0]
    assert client.credentials == ("test-user", "test-password")
    assert client.ca_cert_path == str(tmp_path / "ca.crt")
    assert client.connection == ("broker.example.invalid", 8883, 60)
    assert client.loop_started is True
    assert client.on_connect is mqtt_receiver.on_connect
    assert client.on_message is mqtt_receiver.on_message
    assert client.on_disconnect is mqtt_receiver.on_disconnect


def test_on_connect_subscribes_to_expected_topics():
    client = FakeSubscriber()

    mqtt_receiver.on_connect(client, None, None, 0, None)

    assert client.topics == ["building/room-a/climate/#", "esp-test/#"]


def test_failed_connection_is_logged(caplog):
    client = FakeSubscriber()

    with caplog.at_level(logging.WARNING):
        mqtt_receiver.on_connect(client, None, None, 5, None)

    assert client.topics == []
    assert "Connection failed" in caplog.text
