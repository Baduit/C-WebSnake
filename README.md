# C-WebSnake
A trivial snake done in C++ using Cheerp to compile into javascript to run it on your browser.

# How to compile
Run `/opt/cheerp/bin/clang++ -std=c++1z -target cheerp -O2 main.cpp -o main.js`
On Windows the path of the compiler is different but arguments are the same.

You must have Cheep to compile it: https://github.com/leaningtech/cheerp-meta

But if you can't or don't want to there is already the output file (main.js) in the repo.

# How to run
Open the file main.html with favorite browser (Firefox of course) like that.
`firefox main.html`
