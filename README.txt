Ryne Uemura
HW4 README.txt

This assignment was done to practice understanding concurrency using UNIX threads (pthreads).  It is a simple number 
guessing game where the winning number is stored on a server and the user(s) tries to guess the number.  If the user's
guess was wrong, the server will send back a 'closeness' score which is the difference added up to the user for their next
guess.  When the right number is guessed, the server will end the game for anyone playing and the victory message will 
be displayed for everyone playing.  There is also a leaderboard stored and displayed for the game. This game is assumed
to be run on a Linux machine.

