package client

import (
	"fmt"
	"net/http"
	"strings"
	"strconv"
)

type RukminiConfig struct {
	Host string
}

var allWidthSize = [3]int{500, 600, 700}
var allHeightSize = [3]int{500, 500, 100}

func (this *RukminiConfig) WarmUpCache(url string) bool {
	done := make(chan bool, len(allWidthSize))
	for idx := 0; idx < len(allWidthSize); idx++ {
		newUrl := strings.Replace(url, "{host}", this.Host, 1)
		newUrl = strings.Replace(newUrl, "{width}", strconv.Itoa(allWidthSize[idx]), 1)
		newUrl = strings.Replace(newUrl, "{height}", strconv.Itoa(allHeightSize[idx]), 1)
		fmt.Println(newUrl)
		go func() {
			resp, err := http.Get(newUrl)
			if err != nil || (resp.StatusCode != http.StatusOK && resp.StatusCode != http.StatusNotModified) {
				fmt.Println("NO..........URL=", newUrl, " Warmup in rukmini failed")
				done <- false
			} else {
				fmt.Println("YES..........URL=", newUrl, " Warmup in rukmini failed")
				done <- true
			}
		}()
	}
	for idx := 0; idx < len(allWidthSize); idx++ {
		if !(<- done) {
			fmt.Println("Failure|RukminiWarmUp|URL=", url, " Warmup in rukmini failed")
			return false
		}
	}
	fmt.Println("Success|RukminiWarmUp|URL=", url, " Warmup in rukmini success")
	return true
}

