# mirror_server_c

In this client-server project, a client can request a file or a set of files from the server.<br />
The server searches for the file/s in its file directory rooted at its ~ and returns the tar.gz of the<br />
file/files requested to the client (or an appropriate message otherwise).<br />
Multiple clients can connect to the server from different machines and can request file/s as<br />
per the commands listed in section 2<br />
The server, the mirror and the client processes must run on different machines and<br />
must communicate using sockets only.<br />
<strong>Section 1 (Server)</strong><br />
    <ul>
    <li>The server and an identical copy of the server called the mirror [see section 3] must<br />
        both run before any of the client (s) run and both of them must wait for request/s<br />
        from client/s</li><br />
    <li>Upon receiving a connection request from a client, the server forks a child process<br />
        that services the client request exclusively in a function called processclient() and<br />
        (the server) returns to listening to requests from other clients.<br />
        <ul>
        <li>The processclient() function enters an infinite loop waiting for the client to<br />
            send a command</li><br />
        <li>Upon the receipt of a command from the client, processclient() performs the<br />
            action required to process the command as per the requirements listed in<br />
            section 2 and returns the result to the client</li><br />
    </li>
    <li>Upon the receipt of quit from the client, processclient() exits.</li><br />
    <li>Note: for each client request, the server must fork a separate process with the<br />
        processclient() function to service the request and then go back to listening to<br />
        requests from other clients</li><br />
    </ul>
<strong>Section 2 (Client)</strong><br />
    The client process runs an infinite loop waiting for the user to enter one of the commands.<br />
    Note: The commands are not Linux commands and are defined(in this project) to denote the<br />
    action to be performed by the server.<br />
    Once the command is entered, the client verifies the syntax of the command and if it is okay,<br />
    sends the command to the server, else it prints an appropriate error message.<br />
    List of Client Commands:<br />
        findfile filename<br />
        If the file filename is found in its file directory tree rooted at ~, the server must<br />
        return the filename, size(in bytes), and date created to the client and the<br />
        client prints the received information on its terminal.<br />
        Note: if the file with the same name exists in multiple folders in the<br />
        directory tree rooted at ~, the server sends information pertaining to<br />
        the first successful search/match of filename<br />
        Else the client prints “File not found”<br />
        Ex: C$ findfile sample.txt<br />
        sgetfiles size1 size2 <-u><br />
        The server must return to the client temp.tar.gz that contains all the files in<br />
        the directory tree rooted at its ~ whose file-size in bytes is >=size1 and <=size2<br />
        size1 < = size2 (size1>= 0 and size2>=0)<br />
        -u unzip temp.tar.gz in the pwd of the client<br />
        Ex: C$ sgetfiles 1240 12450 -u<br />
        dgetfiles date1 date2 <-u><br />
        The server must return to the client temp.tar.gz that contains all the files in the<br />
        directory tree rooted at ~ whose date of creation is >=date1 and <=date2<br />
        (date1<=date2)<br />
        -u unzip temp.tar.gz in the pwd of the client<br />
        Ex: C$ dgetfiles 2023-01-16 2023-03-04 -u<br />
        getfiles file1 file2 file3 file4 file5 file6 <-u ><br />
        The server must search the files (file 1 ..up to file6) in its directory tree rooted<br />
        at ~ and return temp.tar.gz that contains at least one (or more of the listed<br />
        files) if they are present<br />
        If none of the files are present, the server sends “No file found” to the client<br />
        (which is then printed on the client terminal by the client)<br />
        -u unzip temp.tar.gz in the pwd of the client<br />
        Ex: C$ getfiles new.txt ex1.c ex4.pdf<br />
        gettargz <extension list> <-u> //up to 6 different file types<br />
        the server must return temp.tar.gz that contains all the files in its directory tree<br />
        rooted at ~ belonging to the file type/s listed in the extension list, else the<br />
        server sends the message “No file found” to the client (which is printed on the<br />
        client terminal by the client)<br />
        -u unzip temp.tar.gz in the pwd of client<br />
        The extension list must have at least one file type and can have up to six<br />
        different file types<br />
        Ex: C$ gettargz c txt pdf<br />
        quit<br />
        The command is transferred to the server and the client process is terminated<br />
    Note:<br />
    It is the responsibility of the client process to verify the syntax of the command<br />
    entered by the user (as per the rules in Section 3) before processing it.<br />
    Appropriate messages must be printed when the syntax of the command is<br />
    incorrect.<br />
    It is the responsibility of the client process to unzip the tar files whenever the option<br />
    is specified.<br />
    <strong>Section 3 Alternating Between the Server and the Mirror</strong><br />
    The server and the mirror (the server’s copy possibly with a few<br />
    additions/changes) are to run on two different machines/terminals.<br />
    The first 4 client connections are to be handled by the server.<br />
    The next 4 client connections are to be handled by the mirror.<br />
    The remaining client connections are to be handled by the server and the<br />
    mirror in an alternating manner- (ex: connection 9 is to be handled by the<br />
    server, connection 10 by the mirror, and so on)<br />
