package main

import (
	"bufio"
	"context"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"strings"

	"github.com/mattn/go-colorable"
	"github.com/tarm/serial"
	"golang.org/x/sync/semaphore"
)

const (
	norm   = "\033[0m"
	yellow = "\033[33m"
	green  = "\033[32m"
	blue   = "\033[34m"
	red    = "\033[31m"
)

var (
	port       = flag.String("port", "/dev/serial", "the serial port path")
	file       = flag.String("input", "", "the file to be send")
	bufferSize = flag.Int("buffer-size", 5, "the resending buffer size")
)

var sendLogger = log.New(colorable.NewColorableStdout(), yellow+"<--- ", 0)
var recvLogger = log.New(colorable.NewColorableStdout(), blue+"---> ", 0)
var erroLogger = log.New(colorable.NewColorableStdout(), red+"-!!- ", 0)
var normLogger = log.New(colorable.NewColorableStdout(), norm+"---- ", 0)
var succLogger = log.New(colorable.NewColorableStdout(), green+"-ok- ", 0)

var lb LineBuffer

func main() {
	// init
	flag.Parse()
	lb = newLineBuffer(*bufferSize)
	c := &serial.Config{Name: *port, Baud: 115200}

	normLogger.Printf("open serial %s:%d", c.Name, c.Baud)
	s, err := serial.OpenPort(c)
	if err != nil {
		erroLogger.Fatal(err)
	}

	normLogger.Println("reset line number")
	fmt.Fprintln(s, "M110 N0")

	go func() {
		// 发送文件
		if *file != "" {
			f, err := os.Open(*file)
			if err != nil {
				erroLogger.Print("open file error: ", err)
			}
			scanner := bufio.NewScanner(f)
			for scanner.Scan() {
				gcode := scanner.Bytes()
				if err := lb.SendLine(s, gcode); err != nil {
					erroLogger.Fatal(err)
				}
			}
			succLogger.Print("file sending complete")
		}

		scanner := bufio.NewScanner(os.Stdin)
		for scanner.Scan() {
			line := scanner.Bytes()
			if err := lb.SendLine(s, line); err != nil {
				erroLogger.Fatal(err)
			}
		}
		if err := scanner.Err(); err != nil {
			erroLogger.Fatal(err)
		}
	}()

	// 打印串口接收到到数据
	r := bufio.NewReader(s)
	for {
		line, _, err := r.ReadLine()
		if err != nil {
			recvLogger.Fatal(err)
		}
		strline := string(line)

		if strings.HasPrefix(strline, "//") {
			recvLogger.Println(green + strline)
		} else if strings.HasSuffix(strline, "ok") {
			lb.Ok()
			recvLogger.Println(green + strline)
		} else if strings.HasSuffix(strline, "rs") { // resend request
			recvLogger.Println(red + strline)
		} else if strings.HasSuffix(strline, "!!") {
			recvLogger.Println(red + strline)
		} else {
			recvLogger.Println(strline)
		}

	}
}

// LineBuffer store latest N lines of command providing error-resending.
type LineBuffer struct {
	lastLineCode int

	b     [][]byte
	limit *semaphore.Weighted
}

func newLineBuffer(size int) LineBuffer {
	return LineBuffer{
		b:     make([][]byte, size),
		limit: semaphore.NewWeighted(int64(size)),
	}
}

func (l *LineBuffer) SendLine(s io.Writer, gcode []byte) error {
	// spends a token if this is G code
	// (means that we need a "ok" reply)
	if len(gcode) > 0 && gcode[0] == 'G' {
		l.limit.Acquire(context.TODO(), 1)
	}

	prefix := fmt.Sprintf("N%d %s", l.lastLineCode+1, gcode)
	l.lastLineCode++
	// Calculate CRC
	var cs byte
	for i := range prefix {
		cs = cs ^ prefix[i]
	}
	pl := fmt.Sprintf("%s*%d\n", prefix, cs)
	sendLogger.Print(pl)
	_, err := s.Write([]byte(pl))
	return err
}

func (l *LineBuffer) Ok() {
	l.limit.Release(1)
}
