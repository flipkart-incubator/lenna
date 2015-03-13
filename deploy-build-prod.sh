#!/bin/bash -e
for APINODE in 3 4 5 11 12
do
    API_HOST="cdn-img"${APINODE}".nm.flipkart.com"
    echo "Deploying build on "${API_HOST}
    ssh ${API_HOST} "sudo apt-get update; sudo apt-get install rukmini=$1"
    echo "Deployed build on "${API_HOST}
done
for APINODE in 2 3 4 5 6 7 8
do
    API_HOST="mobile-rukmini"${APINODE}".nm.flipkart.com"
    echo "Deploying build on "${API_HOST}
    ssh ${API_HOST} "sudo apt-get update; sudo apt-get install rukmini=$1"
    echo "Deployed build on "${API_HOST}
done
for APINODE in {1..18}
do
    size=${#APINODE}
    if [ $size -eq 1 ]
    then
      APINODE=000$APINODE;
    elif [ $size -eq 2 ]
    then
      APINODE=00$APINODE;
    elif [ $size -eq 3 ]
    then
      APINODE=0$APINODE;
    else
      APINODE=$APINODE;
    fi

    API_HOST="mobile-rukmini-app-"${APINODE}".nm.flipkart.com"
    echo "Deploying build on "${API_HOST}
#    ssh ${API_HOST} "sudo apt-get update; sudo apt-get install rukmini=$1"
    echo "Deployed build on "${API_HOST}
done

echo "Done"
echo
