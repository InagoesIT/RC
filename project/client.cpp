#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <cmath>
#include <string>
#include <iostream>

using namespace std;

const int PORT = 2024; 
const char * ADRESS = "127.0.0.1";
const int LOGIN_INFO_SIZE = 50;
const int MSG_SIZE = 600;

int errno;
int sock; // socket descriptor
char msg[MSG_SIZE] = "\0";
string username;
string password;
string _move;

void exitWithErr(string errorMessage);
void getMove();
bool isMoveValid ();
int isLoginInfoInvalid(string info);
void getLoginInfo();
bool isInputYesOrNo(string input);
int getNumber(string input);
void getLeaderboard(bool isEnd);


int main () 
{
    struct sockaddr_in server; // data for connecting to server
    char oppName[LOGIN_INFO_SIZE] = "\0";
    bool isLogged = false;
    char loginInfo[LOGIN_INFO_SIZE * 2] = "\0";

    // creating the socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
        exitWithErr ("[client] Couldn't create the socket. ");
        exit (errno);
    }

    // clean the struct
    bzero (&server, sizeof (server));

    // filling the needed struct for communicating with the server
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr (ADRESS);
    server.sin_port = htons (PORT); // any free port chosen
    bool isNotConnected = true;

    // LOGIN
    
    cout << "[client] You need to log in or register in order to play a game." << endl;
    
    while (!isLogged)
    {
        // getting login info from the client
        getLoginInfo();

        if (isNotConnected)
        {
            // connecting to the server
            if (connect (sock, (struct sockaddr * ) & server, sizeof(struct sockaddr)) == -1) 
            {
                perror ("[client] Couldn't connect to the server. ");
                exit (errno);
            }
            isNotConnected = false;
        }

        cout << "[server] Now you are connected to the server. Yay!" << endl;

        // send login info to server
        string loginInfoAux = username + " " + password;
        strcpy(loginInfo, loginInfoAux.c_str());

        if (write(sock, loginInfo, strlen(loginInfo)) <= 0 )
            exitWithErr("[client] Couldn't write login info to server.");

        cout << "[client] Please wait..." << endl;

        // check if login was successful
        memset(msg, 0, MSG_SIZE*sizeof(msg[0]));
        if (read(sock, msg, MSG_SIZE) <= 0 )
            exitWithErr("[client] Couldn't read the login result from server.");
        if (strcmp(msg, "username or password wrong"))
            isLogged = true;
        else
            cout << "[server] Username or password incorrect! Try again." << endl;
    }

    // tell the client if they were registered or logged in
    if (!strcmp(msg, "login"))
        cout << "[server] Logged successfully." << endl;
    else if (!strcmp(msg, "register"))
        cout << "[server] Registered successfully." << endl;
    else if (!strcmp(msg, "opponent down"))
    {
        cout << "Sorry, your opponent disconnected..." << endl;
        close(sock);
        exit(0);
    }

    cout << "[client] Please wait while the server finds an opponent..." << endl;

    // find out if the server found an opponent
    bzero(oppName, LOGIN_INFO_SIZE);
    if (read(sock, oppName, MSG_SIZE) <= 0 )
        exitWithErr("[client] Couldn't read the opponent result from server.");

    cout << "[server] We found you an opponent to play with." << endl
            << "[server] Your opponent's name is: " << oppName << endl;

    // LEADERBOARD

    getLeaderboard(false);
    
    if (!strcmp(msg, "opponent down"))
        exitWithErr("[server] Sorry! Your opponent disconnected and the game has ended.\n[client] Now you will be disconnected from the server. Bye!");

    char order[20];
    char color[6];

    if (msg[0] == '0')
        strcpy(order, "first");
    else if (msg[0] == '1')
        strcpy(order, "second");
    if (msg[1] == '0')
        strcpy(color, "black");
    else if (msg[1] == '1')
        strcpy(color, "white");

    cout << "[server] You will be the " << order << " playing, and your color will be " << color << "." << endl;
	cout << "[client] ~The game has started!~" << endl;

    bzero(msg, MSG_SIZE);
    if (read(sock, msg, MSG_SIZE) <= 0)
        exitWithErr("[client] Couldn't read info from server.");    

    char moveCh[3];

    while(strcmp(msg, "end"))
    {
        if (!strcmp(msg, "opponent down"))
        {                        
            cout << "[server] Sorry! Your opponent disconnected and the game has ended." << endl 
                << "[client] Now you will be disconnected from the server. Bye!" << endl;
            close(sock);
            exit(0);
        }

        else if (!strcmp(msg, "move"))
        {
            while (1)
            {
                getMove();

                bzero(moveCh, 3);
                strcpy(moveCh, _move.c_str());
                if (write(sock, moveCh, 3) <= 0)
                    exitWithErr("[client] Couldn't write to server.\n[client] Now you will be disconnected from the server. Bye!");

                // find out if the move was successfully done
                bzero(msg, MSG_SIZE);
                if (read(sock, msg, MSG_SIZE) <= 0)
                    exitWithErr("[client] Couldn't read info from server.");

                if (!strcmp(msg, "success"))
                {
                    cout << "[server] Moved successfully." << endl;
                    break;
                }

                cout << "[server] Move impossible! Try again." << endl;
            }
        }
        else if (!strcmp(msg, "skip"))
        {
            cout << "[server] You don't have any available moves at the moment." << endl;
        }

        cout << "[server] Please wait while your opponent decides upon a move." << endl;

        // read the current situation of the game
        bzero(msg, MSG_SIZE);
        if (read(sock, msg, MSG_SIZE) <= 0)
            exitWithErr("[client] Couldn't read info from server.");
    }

    cout << "[server] ~The game has ended!~" << endl;

    // find out who won
    bzero(msg, MSG_SIZE);
    if (read(sock, msg, MSG_SIZE) <= 0)
        exitWithErr("[client] Couldn't read info from server.");

    if (!strcmp(msg, "draw"))
        cout << "[server] The game is a draw." << endl;
    else if (!strcmp(msg, "won"))
        cout << "[server] You won. Congratulations!!!" << endl;
    else 
        cout << "[server] This time you didn't win. Good luck next time!" << endl;

    getLeaderboard(true);

    cout << "[client] Thank you!" << endl
         << "[server] See you soon!" << endl
         << "[client] Now you will be disconnected from the server." << endl;
    
    // closing the conection
    close (sock);

    exit(0);
}


void exitWithErr (string errorMessage)
{
    perror (errorMessage.c_str());
    close(sock);
    exit (errno);
}

void getMove()
{
    for(;;)
    {
        cout << "[client] Please type a valid move." << endl;
        cout << "-> ";
        cin >> _move;
        if (isMoveValid())
            break;

        cout << "[client] Please type a move with the correct format: first character must be a letter from A to H, and the second a number from 1 to 8!" << endl;
        cout << "[client] Try again..." << endl;
    }
    
}

bool isMoveValid ()
{
    if (_move.length() == 0)
        return false;
    if (_move.at(0) >= 'A' && _move.at(0) <= 'H' && _move.at(1) >= '1' && _move.at(1) <= '8')
        return true;
    return false;

}

int isLoginInfoInvalid(string info)
{
    if ((info.length() > LOGIN_INFO_SIZE && info.find(' ') != string::npos) || !info.length())
        return 1;
    else if (info.length() > LOGIN_INFO_SIZE)
        return 2;
    else if (info.find(' ') != string::npos)
        return 3;

    return 0;
}

void getLoginInfo()
{
    int errorNr;
    int i = 0;
    string info;
    string infoType = "username";

    for ( ; i < 2; i++)
    {
        cout << "[client] Type your " << infoType << 
            " containing no spaces with maximum " << LOGIN_INFO_SIZE <<" characters..." << endl;
        cout << "-> ";
        getline (cin, info);

        while ( (errorNr = isLoginInfoInvalid(info)) )
        {
            if (errorNr == 1)
                cout << "[client] Your " << infoType << " can't have more than " << LOGIN_INFO_SIZE <<" characters or spaces!" << endl;
            else if (errorNr == 2)
                cout << "[client] Your " << infoType << " can't have more than " << LOGIN_INFO_SIZE <<" characters!" << endl;
            else 
                cout << "[client] Your " << infoType << " can't contain spaces!" << endl;

            cout << "[client] Type another  " << infoType << "..." << endl;
            cout << "-> ";
            getline (cin, info);
        }

        if (i == 0)
            username = info;
        infoType = "password";
    }
    password = info;
}

bool isInputYesOrNo(string input)
{
    if (input.length() > 1)
        return 0;
    if (input.at(0) == 'y' || input.at(0) == 'n' || input.at(0) == 'Y' || input.at(0) == 'N')
        return 1;
    return 0;
}

void getLeaderboard(bool isEnd)
{
    string input;
    int nrLeaderboard;

    cout << "[client] Do you want to see the leaderboard?" << endl << "-> [y/n] ";
    cin >> msg;

    while (!isInputYesOrNo(msg))
    {
        cout << "[client] You must type 'y', 'Y' or 'N', 'n'! Try again." << endl;
        cout << "[client] Do you want to see the leaderboard?" << endl << "-> [y/n] ";
        cin >> msg;
    }

    // send the answer to the server
    if (write(sock, msg, MSG_SIZE) <= 0 )
        exitWithErr("[client] Couldn't write leaderboard preference to server.");

    // finding out the nr of the leaders wanted
    if (!strcmp(msg, "y") || !strcmp(msg, "Y"))
    {
        cout << "[client] How many users do you want to see from the leaderboard?" << endl;
        cout << "-> ";
        cin >> input;
        nrLeaderboard = getNumber(input);

        while (!nrLeaderboard)
        {
            cout << "[client] Please type a valid number! Try again." << endl;
            cout << "[client] How many users do you want to see from the leaderboard?" << endl;
            cout << "-> ";
            cin >> input;
            nrLeaderboard = getNumber(input);         
        }     

        bzero(msg, MSG_SIZE);
        strcpy(msg, input.c_str());
        // send the answer to the server
        if (write(sock, msg, MSG_SIZE) <= 0 )
            exitWithErr("[client] Couldn't write n for leaderboard to server.");
        
        cout << "[client] Please wait..." << endl;

        // read and display chunks of the result
        memset(msg, 0, MSG_SIZE*sizeof(msg[0]));
        if (read(sock, msg, MSG_SIZE) <= 0 )
            exitWithErr("[client] Couldn't read the leaderboard result from server.");
        
        cout << "[server] The leaderboard is: " << endl;
        cout << msg;
        memset(msg, 0, MSG_SIZE*sizeof(msg[0]));
        do
        {
            memset(msg, 0, MSG_SIZE*sizeof(msg[0]));
            read(sock, msg, MSG_SIZE);
            if (strlen(msg) > 2)
                cout << msg;
        } while (strlen(msg) > 2);
    }
    if (!isEnd)
    {
        cout << "[client] Please wait while the server assigns the order and colors..." << endl;
        if (!strcmp(msg, "n") || !strcmp(msg, "N"))
        {            
            memset(msg, 0, MSG_SIZE*sizeof(msg[0]));
            if (read(sock, msg, MSG_SIZE) <= 0 )
                exitWithErr("[client] Couldn't read order and colors from server.");
        }
    }
}

int getNumber(string input)
{
    int res = 0;

    for (int i = input.length() - 1, j = 0; i >= 0 ; i--, j++)
    {
        if (input.at(i) < '0' || input.at(i) > '9')
            return 0;
        
        res += (input.at(i) - '0') * pow(10, j);
    }

    return res;
}