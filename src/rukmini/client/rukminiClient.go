package client

import (
	"fmt"
	"net/http"
	"strings"
	"strconv"
)

type RukminiConfig struct {
}

var allWidthSize = [3]int{500, 600, 700}
var allHeightSize = [3]int{500, 500, 100}
var allRukminiHost = [3]string{"rukmini1.flixcart.com", "rukmini2.flixcart.com", "rukmini3.flixcart.com"}

func (this *RukminiConfig) WarmUpCache(url string) bool {
//	done := make(chan bool, len(allWidthSize))
	done := 1
	for idx := 0; idx < len(allWidthSize); idx++ {
		newUrl := strings.Replace(url, "{host}", allRukminiHost[idx], 1)
		newUrl = strings.Replace(newUrl, "{width}", strconv.Itoa(allWidthSize[idx]), 1)
		newUrl = strings.Replace(newUrl, "{height}", strconv.Itoa(allHeightSize[idx]), 1)
		fmt.Println(newUrl)
//		go func() {
			resp, err := http.Get(newUrl)
			fmt.Println("URL=", newUrl, " Status", resp.StatusCode)
			if err != nil || (resp.StatusCode != http.StatusOK && resp.StatusCode != http.StatusNotModified) {
				fmt.Println("Failure|RukminiWarmUp|URL=", url, " Warmup in rukmini success")
				done = done & 0
			} else {
				fmt.Println("Success|RukminiWarmUp|URL=", newUrl, " Warmup in rukmini success")
				done = done & 1
			}
//		fmt.Println("Status", done)
//		}()
	}
//	for result := range done {
//		if !result {
//			fmt.Println("Failure|RukminiWarmUp|URL=", url, " Warmup in rukmini failed")
//			return false
//		}
//	}

	if done == 1 {
		fmt.Println("Success|RukminiWarmUp|URL=", url, " Warmup in rukmini success")
		return true
	} else {
		fmt.Println("Failure|RukminiWarmUp|URL=", url, " Warmup in rukmini failed")
		return false
	}
}

