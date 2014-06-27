package client

import (
	"fmt"
	"net/http"
)

func WarmUpCache(url string) bool {
	resp, err := http.Get(url)
	fmt.Println(resp)
	fmt.Println(err)
	if err != nil || resp.StatusCode != http.StatusOK {
		fmt.Println("Failure|RukminiWarmUp|URL=", url, " Warmup in rukmini failed")
		return false
	}
	fmt.Println("Success|RukminiWarmUp|URL=", url, " Warmup in rukmini success")
	return true
}

