WebRealTetris
=============

A HTML5 implementation of RealTetris, a falling block puzzle game.

Try it now on your browser!
http://msakuta.github.io/WebRealTetris/RealTetris.html


Overview
--------

RealTetris is a Tetris inspired variant, which has real fraction of sizes for
the blocks (actually, they are snapped to pixels, so it's not technically
'real').
I know Tetris uses tetromino for blocks, so this game should not be called
'Tetris' in this regard, but you get the idea :)


Objectives
----------

Like Tetris, your goal is to erase as many blocks as possible by aligning them
horizontally before blocks stack up to the top of the stage.
Although, it is almost impossible to fill all gaps between blocks in a row,
so you only have to fill certain fraction of gaps to erase blocks.
The default setting is that 15% or less gap erases the blocks.


Controls
--------

If you've ever played Tetris or its variants, you already know.

* Left, right arrow keys move the falling block horizontally.
* Down key accelerates falling speed of current block.
* Up key rotates current block.
* 'P' key or 'Pause' button toggles pause state of the game.


History
-------

This game's idea had been conceived almost ten years ago.
I have implemented the game with C and a graphic library called WinGraph,
which is limited to Windows platform.
The web was not as much dependent on JavaScript and there were no portable
canvases back then, so making the game with native code was reasonable.

But now, everything is implemented on the web.
I don't know how long I can maintain the source code and library for a native
application, especially when I want to make it portable to other platforms.
So I decided to port the game to HTML5 with canvas.


Libraries
---------

This project uses CreateJS/EaselJS JavaScript library.
