import socket
import threading
import time
import sys

HOST = "127.0.0.1"
PORT = 4242  # Match test invocation
MAX_CLIENTS = 5

def create_client():
    print("create_client: attempting connection")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)  # Increased for stability
        s.connect((HOST, PORT))
        print("create_client: connect succeeded")
        try:
            s.sendall(b"test\n")
            print("create_client: send succeeded")
            return s
        except socket.error as e:
            print(f"create_client: send failed - {e}")
            s.close()
            raise
    except socket.error as e:
        print(f"create_client: connection failed - {e}")
        raise

def receive_all(sock, timeout=1):
    sock.settimeout(timeout)
    data = ""
    try:
        while True:
            chunk = sock.recv(1024).decode(errors="ignore")
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    except socket.error:
        pass
    return data

def test_max_clients():
    print("\n=== Test 1: Maximum Clients ===")
    clients = []
    success = True
    try:
        for i in range(MAX_CLIENTS):
            s = create_client()
            clients.append(s)
            print(f"Connected client {i + 1}")
            time.sleep(0.2)  # Increased to avoid races
        try:
            s = create_client()
            print("Extra client connected (FAIL)")
            success = False
            clients.append(s)
        except socket.error as e:
            print(f"Extra client rejected (PASS) - {e}")
    except socket.error as e:
        print(f"Failed to connect clients - {e}")
        success = False
    finally:
        for s in clients:
            if s:
                s.close()
    print(f"Test 1: {'PASS' if success else 'FAIL'}")
    return success

def test_client_count_after_disconnect():
    print("\n=== Test 2: Client Count After Disconnects ===")
    clients = []
    success = True
    try:
        for i in range(MAX_CLIENTS):
            try:
                s = create_client()
                clients.append(s)
                print(f"Connected client {i + 1}")
                time.sleep(0.2)
            except socket.error as e:
                print(f"Failed to connect client {i + 1} - {e}")
                success = False
                return success
        for i in range(MAX_CLIENTS - 1):
            if clients[i]:
                clients[i].close()
                clients[i] = None
                print(f"Disconnected client {i + 1}")
                time.sleep(0.2)
        new_clients = []
        for i in range(MAX_CLIENTS - 1):
            try:
                s = create_client()
                new_clients.append(s)
                print(f"Reconnected client {i + 1}")
                time.sleep(0.2)
            except socket.error as e:
                print(f"Failed to reconnect client {i + 1} - {e}")
                success = False
                break
        try:
            s = create_client()
            print("Extra client connected (FAIL)")
            success = False
            new_clients.append(s)
        except socket.error as e:
            print(f"Extra client rejected (PASS) - {e}")
        for s in new_clients:
            if s:
                s.close()
    except socket.error as e:
        print(f"Error during test - {e}")
        success = False
    finally:
        for s in clients:
            if s:
                s.close()
    print(f"Test 2: {'PASS' if success else 'FAIL'}")
    return success

def test_shutdown():
    print("\n=== Test 3: Server Shutdown ===")
    try:
        client1 = create_client()
        client2 = create_client()
    except socket.error as e:
        print(f"Failed to create clients - {e}")
        return False
    success = True
    try:
        client1.sendall(b"Test before shutdown\n")
        print("Sent message from client 1")
        time.sleep(1)
        print("Please press Ctrl + C in server terminal")
        time.sleep(5)
        try:
            client1.sendall(b"After shutdown\n")
            print("Client 1 still connected (FAIL)")
            success = False
        except socket.error:
            print("Client 1 detected shutdown (PASS)")
        try:
            client2.sendall(b"After shutdown\n")
            print("Client 2 still connected (FAIL)")
            success = False
        except socket.error:
            print("Client 2 detected shutdown (PASS)")
    except socket.error:
        print("Error during shutdown test")
        success = False
    finally:
        client1.close()
        client2.close()
    print(f"Test 3: {'PASS' if success else 'FAIL'}")
    return success

def main():
    if len(sys.argv) > 1:
        global PORT
        PORT = int(sys.argv[1])
    print(f"Testing server at {HOST}:{PORT}")
    results = []
    results.append(test_max_clients())
    print("Please restart the server for Test 2 if it was stopped")
    input("Press Enter when ready...")
    results.append(test_client_count_after_disconnect())
    results.append(test_shutdown())
    print("\n=== Test Summary ===")
    for i, passed in enumerate(results, 1):
        print(f"Test {i}: {'PASS' if passed else 'FAIL'}")
    if all(results):
        print("All tests passed!")
    else:
        print("Some tests failed.")
if __name__ == "__main__":
    main()