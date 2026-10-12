"""Read-only WIN68 mode-list query; requires pyhidapi (`pip install hid`)."""
import hid

matches = [d for d in hid.enumerate(0x2E3C, 0xC365)
           if d['usage_page'] == 0xFF1B and d['product_string'] == 'WIN 68 HE']
if len(matches) != 1:
    raise SystemExit(f'Expected one WIN68 vendor HID interface; found {len(matches)}')
print('Detected:', matches[0]['product_string'], 'interface', matches[0]['interface_number'])
device = hid.device()
try:
    device.open_path(matches[0]['path'])
    request = [1, 10] + [0] * 62  # Aether protocol.build_read_light_list()
    if device.write(request) != 64:
        raise SystemExit('Short mode-list request')
    reply = device.read(64, 1200)
    if not reply:
        raise SystemExit('No mode-list reply; close Aether/vendor driver and retry')
    # Some Windows HID readers strip the report ID.
    body = reply[1:] if reply[0] == 1 else reply
    if len(body) < 9 or body[0] != 10 or body[4] < 4 or 5 + body[4] > len(body):
        raise SystemExit(f'Unexpected reply: {bytes(reply).hex(" ")}')
    payload = body[5:5 + body[4]]
    print('Firmware modes:', list(payload[:-4]))
    print('Max speed:', payload[-2], 'max brightness:', payload[-1])
finally:
    device.close()
