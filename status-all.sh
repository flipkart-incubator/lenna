#!/bin/bash -e
for APINODE in 2 3 4 5 11 12
do
    API_HOST="cdn-img"${APINODE}".nm.flipkart.com"
    echo ${API_HOST}
    curl -XGET ${API_HOST}":8081/status" || true
done
for APINODE in 1 2 3 4 5 6 7 8
do
    API_HOST="mobile-rukmini"${APINODE}".nm.flipkart.com"
	echo ${API_HOST}
    curl -XGET ${API_HOST}":8081/status" || true
done
echo "Done"
echo
