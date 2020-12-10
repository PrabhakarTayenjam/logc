<h2>
A fast and threadsafe logging library for multithreaded applications.
</h2>

<br>

<h2>Introduction</h2>
<p>
File I/O is a very expensive process. So, if a logger were to perform a file I/O for every log message, it will hurt the overall performance of the application. A faster way would be to collect the log messages in a buffer and write it to the log file afterwards. This will reduce the numbers of file I/O significantly thereby improving the performance. But if the application crashes before writing the log messages in the buffer to the file, the logs messages will be lost. Logc handles this issue by keeping the log buffer in a shared memory between the logc server and a logc client.
</p>

<h2>What is logc?</h2>
<p>
<b>Logc is a C library for logging for multithreaded applications.</b> It is implemented as a client-server architecture. The logc C client will be the application using logc. The client will write the log messages to a log buffer and the server will write the log messages from the buffer to the log file after a buffer usage threshold is reached. This buffer is implemented as a wait-free ring buffer and is in a shared memory between the client and the server.
</p>

<h2>
Building logc
</h2>

`git clone https://github.com/PrabhakarTayenjam/logc.git`
<br>
`cd logc`
<br>
`make`