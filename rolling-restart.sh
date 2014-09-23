#!/bin/bash -e
for APINODE in 2 3 4 5 11 12
do
    API_HOST="cdn-img"${APINODE}".nm.flipkart.com"
    echo "Restarting service on "${API_HOST}
    ssh ${API_HOST} "sudo /etc/init.d/rukmini stop; sudo /etc/init.d/rukmini start"
    echo "Restarted on "${API_HOST}
done
for APINODE in 1 2 3 4
do
    API_HOST="rukmini"${APINODE}".nm.flipkart.com"
    echo "Restarting service on "${API_HOST}
    ssh ${API_HOST} "sudo /etc/init.d/rukmini stop; sudo /etc/init.d/rukmini start"
    echo "Restarted on "${API_HOST}
done
for APINODE in 1 2 3 4 5 6 7 8
do
    API_HOST="mobile-rukmini"${APINODE}".nm.flipkart.com"
    echo "Restarting service on "${API_HOST}
    ssh ${API_HOST} "sudo /etc/init.d/rukmini stop; sudo /etc/init.d/rukmini start"
    echo "Deployed build on "${API_HOST}
done
echo "Done"
echo