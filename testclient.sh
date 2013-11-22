echo "STORE 3nn.png $(stat -c %s  3nn.png) $(cat 3nn.png)" | ncat 127.0.0.1 8002
