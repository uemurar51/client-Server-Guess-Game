#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <vector>
#include <deque>
#include <ctime>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

using namespace std;

const int MAX_GUESS_VAL = 9999;  //number can only be between 0-9999
const int MAX_DIGIT_SPACES = 4;  //max number of digit places is 4.
const int MAX_LEADERBOARD = 3;   //holds max number of people on leaderboard
const int MAX_ARGS = 3;          //maxiumum number of arguments for commandline
const int PORT_ARG = 2;          //port argument in cmdline input
const int IP_ARG = 1;            //ip address of server as cmdline input

struct leaderboard{
  vector<string> names;
  vector<long> numGuesses;
};

//send and get functions for sending longs/strings over the network
void sendLong(long num, int sock);
int getLong(int sock);
void sendStr(string msg, int sock);
string getStr(int sock);

//leaderboard functions
leaderboard getLBoard(int sock);
void printBoard(leaderboard board);




int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    bool hasWon = false;          //check to see if client won
    int status;                   //check conn functions
    long userGuess, roundCt;      //user guesses and round counter
    string playerNameHolder;      //holds the name of player before sent to serv
    string victoryMsg;            //holds victory message from the server
    bool goodInput;               //if guess is valid or not
    struct leaderboard board;


    //check to see if number or args passed == needed amount
    if (argc != MAX_ARGS)
    {
      cerr << "Invalid number of arguments, input the your IP address for " <<
        "the first arg, then prt # for second.  Now exiting program....";
      exit(0);
    }

    //getting port number from cmdline args
    int portNum = atoi(argv[PORT_ARG]);

    //initializing socket and checking for error
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
      cerr << "Error opening socket.... Exiting program";
      exit(0);
    }
    //getting IP address fron cmdline args
    server = gethostbyname(argv[IP_ARG]);
    if (server == NULL)
    {
      cerr << "Error, no such host... Exiting program";
      exit(0);
    }
    //setting server address members
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(portNum);
    //connect to server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        cerr << "Error connecting to server... Exiting program";
        exit(0);
    }

    //if connected to server, will start the game!
    cout << "You've connected to the server!" << endl;
    cout << "The goal of this game is to guess the number that is saved by " <<
      "the main server" << endl;

    //get the name of the client and send to the server
    cout << "Enter your name: ";
    cin >> playerNameHolder;
    sendStr(playerNameHolder, sockfd);

    cout << "Welcome " << playerNameHolder << "! Guess a number between 0001-"
    << "9999." << endl;

    //set round count to 0
    roundCt = 0;
    //do-while loop for a turn
    do{

      cout << "This is turn " << (roundCt + 1) << " what is your guess? ";
      goodInput = false;

      //check if guess is valid, if not, try again.
      while (!goodInput)
      {
          if(cin >> userGuess && userGuess <= 9999 && userGuess >= 1)
            goodInput = true;
          else
          {
            cout << "Invalid input, please input an integer between 0001-9999."
            << endl;
            cout << "Try again: ";
            cin.clear();
            cin.ignore();
          }
      }
      //user guess was valid, send to server
      sendLong(userGuess, sockfd);

      //get closeness from round
      int resCloseness = getLong(sockfd);
      //checking if user won
      if (resCloseness == 0)
      {
        hasWon = true;
        victoryMsg = getStr(sockfd);
      }
      //if didn't win will just show resulting closeness
      cout << "Closeness of guess: " << resCloseness << endl;
      roundCt++;
    }while(!hasWon);

    //writing victory message to client console
    cout << victoryMsg << endl;


    //getting leaderboard information and displaying it to the screen
    board = getLBoard(sockfd);
    printBoard(board);

    //close socket
    status = close(sockfd);
    if (status < 0)
    {
      cerr << "Error with closing socket" << endl;
      exit(-1);
    }
}

//from here are functions and definitions

leaderboard getLBoard(int sock)
{
  leaderboard tempBoard;
  for(int i = 0; i < MAX_LEADERBOARD; i++)
  {
    tempBoard.numGuesses.push_back(getLong(sock));
    tempBoard.names.push_back(getStr(sock));
  }
  return tempBoard;
}

void printBoard(leaderboard board)
{
  cout << endl << endl;
  cout << "Leaderboard" << endl << "------------" << endl;
  for (int i = 0; i < MAX_LEADERBOARD; i++)
  {
    if (board.numGuesses[i] != 0)
    {
      cout << board.names[i] << ":   " << board.numGuesses[i] << endl;
    }
  }
}

void sendLong(long num, int sock)
{
  long temp = htonl(num);
  int bytesSent = send(sock, (void *) &temp, sizeof(long), 0);
  if (bytesSent != sizeof(long))
    exit(-1); //failed to send.
}

int getLong(int sock)
{
  int bytesLeft = sizeof(long);
  long networkInt;
  char *bp = (char *) &networkInt;

  while (bytesLeft > 0)
  {
    int bytesRecv = recv(sock, (void *) bp, bytesLeft, 0);
    if (bytesRecv <= 0) exit(-1);
    else
    {
      bytesLeft = bytesLeft - bytesRecv;
      bp = bp + bytesRecv;
    }
  }
  long hostInt = ntohl(networkInt);
  return hostInt;
}

//maybe make an argument the strlen of the name
void sendStr(string msg, int sock)
{
  long msgSize = (msg.length());
  sendLong(msgSize, sock);
  int bytesSent = send(sock, msg.c_str(), msg.size(), 0);
  if(bytesSent != msgSize)
    exit(-1);
}

string getStr(int sock)
{
  int bytesLeft = getLong(sock);
  char msg[512];            //could not figure out variable length
  //msg = new char[bytesLeft];
  bzero(msg, bytesLeft);
  char *bp = (char *)&msg;
  string tempStr;
  while(bytesLeft > 0)
  {
    int bytesRecv = recv(sock, (void *)bp, bytesLeft, 0);
    if(bytesRecv <= 0) exit(-1);
    else
    {
      bytesLeft = bytesLeft - bytesRecv;
      bp = bp + bytesRecv;
    }
  }
  tempStr = string(msg);
  return tempStr;
}
