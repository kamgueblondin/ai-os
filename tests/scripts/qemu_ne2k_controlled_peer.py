#!/usr/bin/env python3
"""Pair Ethernet déterministe pour le contrat NE2000 QEMU.

Le backend socket de QEMU transporte chaque trame Ethernet avec un préfixe de
longueur big-endian sur quatre octets. Ce pair n’ouvre aucune connexion externe :
il attribue un bail DHCP local, répond au serveur ARP/DNS local, puis constate le
SYN vers l’adresse A déterministe renvoyée par DNS.
"""
import socket
import struct
import threading

SERVER_MAC = b"\x52\x54\x00\xa0\x20\x02"
SERVER_IP = b"\x0a\x20\x00\x02"       # 10.32.0.2
GUEST_IP = b"\x0a\x20\x00\x0f"        # 10.32.0.15
NETMASK = b"\xff\xff\xff\x00"
REMOTE_IP = b"\xcb\x00\x71\x14"       # 203.0.113.20, TEST-NET-3


def _checksum(data):
    if len(data) & 1:
        data += b"\0"
    total = sum((data[index] << 8) | data[index + 1] for index in range(0, len(data), 2))
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return (~total) & 0xffff


def _ipv4_packet(source_ip, destination_ip, protocol, payload):
    total_length = 20 + len(payload)
    ip = bytearray(20)
    ip[0] = 0x45
    ip[2:4] = struct.pack("!H", total_length)
    ip[6:8] = b"\x40\x00"
    ip[8] = 64
    ip[9] = protocol
    ip[12:16] = source_ip
    ip[16:20] = destination_ip
    ip[10:12] = struct.pack("!H", _checksum(bytes(ip)))
    return bytes(ip) + payload


def _ipv4_udp(source_ip, destination_ip, source_port, destination_port, payload):
    udp_length = 8 + len(payload)
    udp = struct.pack("!HHHH", source_port, destination_port, udp_length, 0)
    return _ipv4_packet(source_ip, destination_ip, 17, udp + payload)


def _ipv4_tcp(source_ip, destination_ip, source_port, destination_port, sequence, acknowledgment, flags, payload=b""):
    tcp = bytearray(20 + len(payload))
    tcp[0:4] = struct.pack("!HH", source_port, destination_port)
    tcp[4:12] = struct.pack("!II", sequence, acknowledgment)
    tcp[12] = 0x50
    tcp[13] = flags
    tcp[14:16] = b"\xff\xff"
    tcp[20:] = payload
    pseudo_header = source_ip + destination_ip + b"\x00\x06" + struct.pack("!H", len(tcp))
    tcp[16:18] = struct.pack("!H", _checksum(pseudo_header + bytes(tcp)))
    return _ipv4_packet(source_ip, destination_ip, 6, bytes(tcp))


def _ethernet(destination_mac, ethertype, payload):
    return destination_mac + SERVER_MAC + struct.pack("!H", ethertype) + payload


def _dhcp_type(payload):
    position = 240
    while position < len(payload):
        code = payload[position]
        position += 1
        if code == 255:
            return 0
        if code == 0:
            continue
        if position >= len(payload):
            return 0
        length = payload[position]
        position += 1
        if position + length > len(payload):
            return 0
        if code == 53 and length == 1:
            return payload[position]
        position += length
    return 0


def _dhcp_reply(request, message_type):
    xid = request[4:8]
    client_mac = request[28:34]
    payload = bytearray(278)
    payload[0:4] = b"\x02\x01\x06\x00"
    payload[4:8] = xid
    payload[16:20] = GUEST_IP
    payload[28:34] = client_mac
    payload[236:240] = b"\x63\x82\x53\x63"
    options = bytearray()
    options += b"\x35\x01" + bytes((message_type,))
    options += b"\x36\x04" + SERVER_IP
    if message_type == 5:  # ACK
        options += b"\x01\x04" + NETMASK
        options += b"\x03\x04" + SERVER_IP
        options += b"\x06\x04" + SERVER_IP
        options += b"\x33\x04" + struct.pack("!I", 3600)
    options += b"\xff"
    payload[240:240 + len(options)] = options
    return _ethernet(b"\xff" * 6, 0x0800, _ipv4_udp(SERVER_IP, b"\xff\xff\xff\xff", 67, 68, bytes(payload)))


def _arp_reply(request):
    sender_mac = request[22:28]
    sender_ip = request[28:32]
    payload = b"\x00\x01\x08\x00\x06\x04\x00\x02" + SERVER_MAC + SERVER_IP + sender_mac + sender_ip
    return _ethernet(sender_mac, 0x0806, payload)


def _dns_reply(request_payload):
    if len(request_payload) < 17:
        return None
    question_end = 12
    while question_end < len(request_payload) and request_payload[question_end] != 0:
        label_length = request_payload[question_end]
        question_end += 1 + label_length
    question_end += 5
    if question_end > len(request_payload):
        return None
    header = bytearray(12)
    header[0:2] = request_payload[0:2]
    header[2:4] = b"\x81\x80"
    header[4:6] = b"\x00\x01"
    header[6:8] = b"\x00\x01"
    answer = b"\xc0\x0c\x00\x01\x00\x01\x00\x00\x00\x3c\x00\x04" + REMOTE_IP
    return bytes(header) + request_payload[12:question_end] + answer


class ControlledEthernetPeer:
    """Serveur socket QEMU non persistant avec compteurs de protocole publics."""

    def __init__(self):
        self.events = {"discover": 0, "offer": 0, "request": 0, "ack": 0,
                       "arp": 0, "dns": 0, "syn": 0, "syn_ack": 0, "client_hello": 0}
        self.error = None
        self._stop = threading.Event()
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listener.bind(("127.0.0.1", 0))
        self._listener.listen(1)
        self._listener.settimeout(0.2)
        self.port = self._listener.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def start(self):
        self._thread.start()
        return self.port

    def close(self):
        self._stop.set()
        try:
            self._listener.close()
        except OSError:
            pass
        self._thread.join(timeout=2)

    @staticmethod
    def _read_exact(connection, length):
        chunks = []
        remaining = length
        while remaining:
            chunk = connection.recv(remaining)
            if not chunk:
                return None
            chunks.append(chunk)
            remaining -= len(chunk)
        return b"".join(chunks)

    @staticmethod
    def _send_frame(connection, frame):
        connection.sendall(struct.pack("!I", len(frame)) + frame)

    def _handle_frame(self, connection, frame):
        if len(frame) < 14:
            return
        ethertype = struct.unpack("!H", frame[12:14])[0]
        if ethertype == 0x0806 and len(frame) >= 42:
            if frame[20:22] == b"\x00\x01" and frame[38:42] == SERVER_IP:
                self.events["arp"] += 1
                self._send_frame(connection, _arp_reply(frame))
            return
        if ethertype != 0x0800 or len(frame) < 42:
            return
        ip_offset = 14
        if (frame[ip_offset] >> 4) != 4:
            return
        header_length = (frame[ip_offset] & 0x0f) * 4
        if header_length < 20 or len(frame) < ip_offset + header_length:
            return
        protocol = frame[ip_offset + 9]
        source_ip = frame[ip_offset + 12:ip_offset + 16]
        if protocol == 6:
            tcp_offset = ip_offset + header_length
            if len(frame) < tcp_offset + 20:
                return
            source_port, destination_port = struct.unpack("!HH", frame[tcp_offset:tcp_offset + 4])
            sequence = struct.unpack("!I", frame[tcp_offset + 4:tcp_offset + 8])[0]
            flags = frame[tcp_offset + 13]
            tcp_header_length = (frame[tcp_offset + 12] >> 4) * 4
            if tcp_header_length < 20 or len(frame) < tcp_offset + tcp_header_length:
                return
            payload = frame[tcp_offset + tcp_header_length:]
            if source_port == 49152 and destination_port == 443 and (flags & 0x12) == 0x02:
                self.events["syn"] += 1
                self._send_frame(connection, _ethernet(frame[6:12], 0x0800,
                    _ipv4_tcp(REMOTE_IP, source_ip, 443, source_port, 0x10203040,
                              sequence + 1, 0x12)))
                self.events["syn_ack"] += 1
            elif source_port == 49152 and destination_port == 443 and len(payload) >= 5 and \
                    payload[0:3] == b"\x16\x03\x03":
                self.events["client_hello"] += 1
            return
        if protocol != 17:
            return
        udp_offset = ip_offset + header_length
        if len(frame) < udp_offset + 8:
            return
        source_port, destination_port, udp_length, _ = struct.unpack("!HHHH", frame[udp_offset:udp_offset + 8])
        if udp_length < 8 or len(frame) < udp_offset + udp_length:
            return
        payload = frame[udp_offset + 8:udp_offset + udp_length]
        if source_port == 68 and destination_port == 67 and len(payload) >= 244:
            kind = _dhcp_type(payload)
            if kind == 1:
                self.events["discover"] += 1
                self._send_frame(connection, _dhcp_reply(payload, 2))
                self.events["offer"] += 1
            elif kind == 3:
                self.events["request"] += 1
                self._send_frame(connection, _dhcp_reply(payload, 5))
                self.events["ack"] += 1
            return
        if source_port == 49152 and destination_port == 53:
            response = _dns_reply(payload)
            if response is not None:
                self.events["dns"] += 1
                self._send_frame(connection, _ethernet(frame[6:12], 0x0800,
                    _ipv4_udp(SERVER_IP, source_ip, 53, 49152, response)))

    def _serve(self):
        connection = None
        try:
            while not self._stop.is_set():
                try:
                    connection, _ = self._listener.accept()
                    break
                except socket.timeout:
                    continue
            if connection is None:
                return
            connection.settimeout(0.2)
            with connection:
                while not self._stop.is_set():
                    try:
                        size_raw = self._read_exact(connection, 4)
                    except socket.timeout:
                        continue
                    if size_raw is None:
                        return
                    size = struct.unpack("!I", size_raw)[0]
                    if size == 0 or size > 2048:
                        raise RuntimeError("invalid Ethernet frame size %d" % size)
                    frame = self._read_exact(connection, size)
                    if frame is None:
                        return
                    self._handle_frame(connection, frame)
        except Exception as error:  # utilisé par le contrat appelant
            self.error = error
