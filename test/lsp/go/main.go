package main;
import (
	"fmt"
)

type X struct {
	y int
}

/// https://example.com
func main() {
	_, err := fmt.Println("hello world");
	if err != nil {
		return;
	}
	var a X
	println(a.y);
	
}
