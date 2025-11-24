import unittest
from scripts import utils


class TestUtils(unittest.TestCase):
    def test_parse_register_key_hex(self):
        self.assertEqual(utils.parse_register_key('0x0A'), 0x0A)
        self.assertEqual(utils.parse_register_key('0xFF'), 0xFF)

    def test_parse_register_key_decimal(self):
        self.assertEqual(utils.parse_register_key('10'), 10)
        self.assertEqual(utils.parse_register_key(5), 5)

    def test_parse_register_key_invalid(self):
        with self.assertRaises(ValueError):
            utils.parse_register_key('x12')

    def test_validate_register_map_ok(self):
        inp = {'0x01': 10, '2': 255}
        out = utils.validate_register_map(inp)
        self.assertEqual(out[1], 10)
        self.assertEqual(out[2], 255)

    def test_validate_register_map_bad_addr(self):
        with self.assertRaises(ValueError):
            utils.validate_register_map({'0x123': 1})

    def test_validate_register_map_bad_value(self):
        with self.assertRaises(ValueError):
            utils.validate_register_map({'0x01': 999})

    def test_validate_mqtt_uri(self):
        r = utils.validate_mqtt_uri('mqtt://example.com:1883')
        self.assertEqual(r['host'], 'example.com')
        self.assertEqual(r['port'], 1883)
        self.assertFalse(r['secure'])

    def test_validate_mqtt_uri_default_port(self):
        r = utils.validate_mqtt_uri('mqtts://example.com')
        self.assertTrue(r['secure'])
        self.assertEqual(r['port'], 8883)

    def test_validate_mqtt_uri_bad(self):
        with self.assertRaises(ValueError):
            utils.validate_mqtt_uri('http://example')


if __name__ == '__main__':
    unittest.main()
