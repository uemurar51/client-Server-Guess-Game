#include <iostream>
#include <stdlib.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <strings.h>
#include <stdio.h>
#include <string>
#include <cmath>
#include <vector>
#include <deque>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <time.h>

using namespace std;

const int MAX_ARGS = 2;         //max arguments for command line input
const int PORT_ARG = 1;         //argument for port
const int MAX_LEADERBOARD = 3;  //max people allowed on leaderboard
const int MAX_DIGIT_SPACES = 4;  //max number of digit places is 4.

struct connInfo{
  int sock;
  int roundCount;
  string name;
};

struct leaderboard{
  vector<string> names;
  vector<long> roundCt;
};

//board functions(initialize, check, insert,send and output)
void initLBoard();
void sendLBoard(leaderboard board, connInfo conn);
void checkLBoard(string name, long round);
void insertLBoard(int index, string name, long rounds);
void outputLBoard(leaderboard board);

//receive and send functions for longs
long getLong(connInfo conn, bool &abortFunc);
void sendLong(long num, connInfo conn);

//send and receive functions for strings
string recvStr(connInfo conn, bool &abortFunc);
void sendStr(string msg, connInfo conn);

//functions use to get closeness for each round
void splitInt(deque<int> &digits, int number);
int getCloseness(deque<int> &actualNum, deque<int> &guess);

//initializing function for global lock
void initLock();

//global variables
leaderboard lBoard;
pthread_mutex_t lbLock;

void *threadFunction(void * args_pa)
{
  //getting variables from the argument
  struct connInfo *args_p;
  args_p = (struct connInfo *)args_pa;

  srand(time(NULL));            //seeding random variable
  args_p->roundCount = 0;       //setting round count to 0
  string clientName;            //used to hold client name
  long roundCount = 0;          //to keep track of rounds
  long numberToGuess;           //used to hold actual number to guess
  long clientGuess;             //used to hold client's guess
  long closeness;               //used to calculate cloesness
  long resultCloseness;         //result to send to client
  deque<int> actualNumDigits;   //the number to guess's digits
  deque<int> clientGuessDigits; //the clients guess's digits
  bool hasWon = false;
  string victoryMsg;            //holds victory message
  bool exitUnit = false;        //bool used in functions to error check
  char buffer[256];             //variables for victory message
  stringstream ss;

  //setting thread to reclaim resources once exited
  pthread_detach(pthread_self());

  //getting clientName from client and putting it in args_p
  clientName = recvStr(*args_p, exitUnit);
  //Welcome msg
  cout << clientName << " has connected." << endl;
  cout << clientName << "'s number is...";

  //generate the random number to guess
  numberToGuess = (rand() % 10000);
  cout << numberToGuess << endl;

  //while loop for turns
  while(!exitUnit && !hasWon)
  {
    args_p-> name = clientName;
    cout << endl << endl;

    do {                        //get the guess from client and compare
      //keep track of closeness
      clientGuess = getLong(*args_p, exitUnit);

      if(!exitUnit)
      {
        cout << "Recieved guess(" << clientName << "): " << clientGuess << endl;
        cout << "Actual number(" << clientName << "): " << numberToGuess << endl;
        splitInt(actualNumDigits, numberToGuess);
        splitInt(clientGuessDigits, clientGuess);
        closeness = getCloseness(actualNumDigits, clientGuessDigits);
      }
      else
      {
        hasWon = true;
        break;
      }
      //set closeness calculated to result variable and outputting the result
      resultCloseness = closeness;
      cout << "Result of round... Closeness: " << resultCloseness << endl;
      sendLong(resultCloseness, *args_p);

      roundCount++;         //round over, increment
      //check if user won, return victoryMsg to client if won and set bool true
      if(closeness == 0)
      {
        hasWon = true;
        bzero(buffer, 256);
        ss.str("");
        ss<<"Congratulations! It took "<<roundCount<<
          " turns to guess the number!"<<endl;
        strcpy(buffer, (ss.str()).c_str());
        victoryMsg = string(buffer);
        //send victory message to user
        sendStr(victoryMsg, *args_p);
        bzero(buffer, 256);
      }
      actualNumDigits.clear();
      clientGuessDigits.clear();

    } while(!hasWon);

    if(!exitUnit)
    {
      cout << clientName << " has won! Sent victory message to client" << endl;

      //locking board access functions to allow only one thread at a time
      pthread_mutex_lock(&lbLock);
      checkLBoard(clientName, roundCount);
      pthread_mutex_unlock(&lbLock);

      //outputting and sending leaderboard
      cout << "Current leaderboard: " << endl;
      outputLBoard(lBoard);
      sendLBoard(lBoard, *args_p);
    }
  }
  if(exitUnit)
  {
    cerr << endl << "User has left prematurely!" << endl;
  }

  //printing that client thread is going to exit, and printing their socket
    //int to the screen
  cout << endl << clientName << " is now exiting, sock " << args_p->sock <<
    " closing" << endl;
  cout << endl << "Waiting for new client...." << endl;
  //close sockets
  close(args_p->sock);
  //exiting pthread, to ensure no memory leak
  pthread_exit(NULL);
}

//MAIN
int main(int argc, char* argv[])
{
  int sockfd, clientsockfd, portNum, status;
  int randNum;                  //used to process clients
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  //check if right aqmount of arguments passed
  if(argc != MAX_ARGS)
  {
    cerr << "Invalid number of arguments. Please input the port number as " <<
      "the only argument.... Exiting program";

    exit(-1);
  }

  //create the socket and check if opened socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
  {
    cerr << "Error with opening socket.... Exiting program";
    close(sockfd);
    exit(-1);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));

  //initiazlize the leaderboard and lock for leaderboard
  initLBoard();
  initLock();

  //setting the port
  portNum = atoi(argv[PORT_ARG]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portNum);

  status = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (status < 0)
  {
    cerr << "Error with bind..... Exiting program" << endl;
    close(sockfd);
    exit(-1);
  }

  //setting server to listen for clients
  status = listen(sockfd, 5);
  cout << "Now listening for a new client to connect to the server..." << endl;
  if(status < 0)
  {
    cerr << "Error with listen.... Exiting program" << endl;
    close(sockfd);
    exit(-1);
  }

  clilen = sizeof(cli_addr);
  //server runs until canceled
  while(true)
  {
    //create thread id to keep track of threads created
    pthread_t tid;

    //accept client and test if there are errors
    clientsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if(clientsockfd < 0)
    {
      cerr << "Error with accept of new client sock.... Exiting program";
      close(clientsockfd);
      exit(-1);
    }

    //setting the connection info in args_p to send when threads are created
    struct connInfo *args_p;
    args_p = new struct connInfo;
    args_p->sock = clientsockfd;

    //create the threads and process them
    status = pthread_create(&tid, NULL, threadFunction, (void *) args_p);
    if (status)
    {
      cerr << "Error creating thread, return code is " << status <<
        " now exiting program" << endl;
      close(clientsockfd);
      exit(-1);
    }
    randNum = (rand() % 101);
    cout << "Client thread started! Random number is: " << randNum << endl;
  }
  return 0;
}

//after this point, are functions and definitions

//splits an integer into seperate digits using deque
void splitInt(deque<int> &digits, int number)
{
  if (number == 0)
  {
    for (int i = 0; i < MAX_DIGIT_SPACES; i++)
    {
      digits.push_front(0);
    }
  }
  else
  {
    while(number != 0)
    {
      int last = number % 10;
      digits.push_front(last);
      number = (number - last) / 10;
    }
    if (digits.size() < MAX_DIGIT_SPACES)
    {
      for (int i = digits.size(); i < MAX_DIGIT_SPACES; i++)
      {
        digits.push_front(0);
      }
    }
  }
}
//gets closeness value for game
int getCloseness(deque<int> &actualNum, deque<int> &guess)
{
  int closeness = 0;
  for(int i = 0; i < MAX_DIGIT_SPACES; i++)
  {
    if (actualNum[i] >= guess[i])
    {
      closeness += actualNum[i] - guess[i];
    }
    else
      closeness += guess[i] - actualNum[i];
  }
  return closeness;
}

long getLong(connInfo conn, bool &abortFunc)
{
  int bytesLeft = sizeof(long);
  long networkInt;
  char *bp = (char *) &networkInt;

  while (bytesLeft > 0)
  {
    int bytesRecv = recv(conn.sock, (void *)bp, bytesLeft, 0);
    if(bytesRecv <= 0)
    {
      abortFunc = true;
      break;
    }
    else
    {
      bytesLeft = bytesLeft - bytesRecv;
      bp = bp + bytesRecv;
    }
  }
  if(!abortFunc)
  {
    long hostInt = ntohl(networkInt);
    return hostInt;
  }
  else
    return 0;
}

void sendLong(long number, connInfo conn)
{
  long temp = htonl(number);
  int bytesSent = send(conn.sock, (void *) &temp, sizeof(long), 0);
  if (bytesSent != sizeof(long))
  {
    cerr << "Error sending long.... exiting";
    close(conn.sock);
    exit(-1);
  }
}

void sendStr(string msg, connInfo conn)
{
  long msgSize = (msg.length() + 1);
  char *msgSend;
  msgSend = new char[msgSize];
  strcpy(msgSend, msg.c_str());

  sendLong(msgSize, conn);
  int bytesSent = send(conn.sock, msg.c_str(), msgSize, 0);
  if(bytesSent != msgSize)
  {
    close(conn.sock);
    exit(-1);
  }
}

string recvStr(connInfo conn, bool &abortFunc)
{
  int bytesLeft = getLong(conn, abortFunc);
    if(!abortFunc)
    {
      //char *msg;                        //could not figure out  how to get
      //msg = new char[bytesLeft];          //get dynamic array to recieve
      char msg[512];
      char *bp = (char *)&msg;
      string temp;
      while(bytesLeft > 0)
      {
        int bytesRecv = recv(conn.sock, (void *)bp, bytesLeft, 0);
        if(bytesRecv <= 0)
        {
          abortFunc = true;
          break;
        }
        else
        {
          bytesLeft = bytesLeft - bytesRecv;
          bp = bp + bytesRecv;
        }
      }
      if(!abortFunc)
      {
        temp = string(msg);
        return temp;
      }
      return "--";
    }
    else
    {
      return "--";
    }
}

void initLBoard()
{
  for(int i = 0; i < MAX_LEADERBOARD; i++)
  {
    lBoard.roundCt.push_back(0);
    lBoard.names.push_back("---");
  }
}

void insertLBoard(int index, string name, long rounds)
{
  if(index == (MAX_LEADERBOARD - 1))
  {
    lBoard.names[index] = name;
    lBoard.roundCt[index] = rounds;
  }
  else
  {
    int roundTemp = rounds;
    string nameTemp = name;

    for(int i = (MAX_LEADERBOARD - 1); i > index; i--)
    {
      lBoard.names[i] = lBoard.names[i-1];
      lBoard.roundCt[i] = lBoard.roundCt[i-1];
    }
    lBoard.names[index] = nameTemp;
    lBoard.roundCt[index] = roundTemp;
  }
}
void checkLBoard(string name, long round)
{
  int indexToPass = 0;
  bool leader = false;

  for(int i = (MAX_LEADERBOARD - 1); i >= 0; i--)
  {
    if(lBoard.roundCt[i] > round || lBoard.roundCt[i] == 0)
    {
      leader = true;
      indexToPass = i;
    }
  }
  if(leader)
    insertLBoard(indexToPass, name, round);
}

void sendLBoard(leaderboard board, connInfo conn)
{
  for(int i = 0; i < MAX_LEADERBOARD; i++)
  {
    sendLong(lBoard.roundCt[i], conn);
    sendStr(lBoard.names[i], conn);
  }
}

void initLock()
{
  pthread_mutex_init(&lbLock, NULL);
}

void outputLBoard(leaderboard board)
{
  cout << endl << endl;
  cout << "Leaderboard" << endl << "-----------" << endl;
  for(int i = 0; i < MAX_LEADERBOARD; i++)
  {
    if(board.roundCt[i] != 0)
      cout << board.names[i] << "    " << board.roundCt[i] << endl;
  }
}
