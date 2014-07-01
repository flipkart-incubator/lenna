package client

import (
	"strings"
	"strconv"
	"net/http"
	"fmt"
)

type RukminiConfig struct {
	Host string
}

var allWidthSize = [3]int{20, 30, 40}
var allHeightSize = [3]int{20, 30, 40}

func (this *RukminiConfig) WarmUpCache(url string) bool {
	done := 1
	for idx := 0; idx < len(allWidthSize); idx++ {
		newUrl := strings.Replace(url, "{host}", this.Host, 1)
		newUrl = strings.Replace(newUrl, "{width}", strconv.Itoa(allWidthSize[idx]), 1)
		newUrl = strings.Replace(newUrl, "{height}", strconv.Itoa(allHeightSize[idx]), 1)
//		fmt.Println(newUrl)
//		go func(idx int) {
			resp, err := http.Get(newUrl)
			fmt.Println("INFO|RukminiWarmUp|URL=", newUrl, " Status", resp.StatusCode)
			if err != nil || (resp.StatusCode != http.StatusOK && resp.StatusCode != http.StatusNotModified) {
				done = done & 0
			} else {
				done = done & 1
			}
//		}(idx)
	}
//	close(done)

	if done == 0 {
//		fmt.Println("Failure|RukminiWarmUp|URL=", url, " Warmup in rukmini failed")
		return false
	} else {
//		fmt.Println("Success|RukminiWarmUp|URL=", url, " Warmup in rukmini success")
		return true
	}
}

