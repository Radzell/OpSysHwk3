printf "APPEND test.txt $(stat -c %s  test.txt) $(cat test.txt)" | ncat 127.0.0.1 8002
