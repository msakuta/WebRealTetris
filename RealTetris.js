'use strict';
(function(){

var canvas;
var width;
var height;
var minimapCanvas;
var stage;
var overlay;
var blockStage;
var moved = false;
var nextx;
var nexty;
var gameOver = false;
var clearall = true;
var gameOverText = null;
var keyState = {
	left: false,
	up: false,
	right: false,
	down: false,
};
var rs = new Xor128();

var MAX_BLOCK_WIDTH = 50;
var MAX_BLOCK_HEIGHT = 50;
var MIN_BLOCK_WIDTH = 10;
var MIN_BLOCK_HEIGHT = 10;
var MAX_DELAY = 25; /* maximum block destroying delay */
var InitBlockRate = 0.5;
var downSpeed = 20;

function Block(l, t, r, b){
	this.l = l;
	this.t = t;
	this.r = r;
	this.b = b; /* dimension of the block. */
//	fixed vx, vy; /* velocity in fixed format. display is in integral coordinates system, anyway. */
	this.life = 0; /* if not UINT_MAX, the block is to be removed after this frames. */
}

Block.prototype.getW = function(){
	return this.r - this.l;
};

Block.prototype.getH = function(){
	return this.b - this.t;
};

/// Test intersection between blocks
Block.prototype.intersects = function(r){
	return this.r < r.l && r.r < this.l && this.b < r.t && r.b < this.t;
};

/// Set left top position of this block without changing size
Block.prototype.set = function(x,y){
	this.r -= this.l - x;
	this.l = x;
	this.b -= this.t - y;
	this.t = y;
};

/// Update position by tracing intersecting blocks downwards
Block.prototype.slipDown = function(ig, dest){
	var ret = block_list.length;
	var min = dest;

	for(var i = 0; i < block_list.length; i++){
		if(i !== ig && this.l < block_list[i].r && block_list[i].l < this.r && this.b <= block_list[i].t && block_list[i].t < min){
			ret = i;
			min = block_list[i].t;
		}
	}

	min -= this.b - this.t;
	this.set(this.l, min);

	return ret;
};

/// Update position by tracing intersecting blocks towards left
Block.prototype.slipLeft = function(ig, dest){
	var ret = block_list.length;
	var max = dest;

	for(var i = 0; i < block_list.length; i++){
		if(i !== ig && this.t < block_list[i].b && block_list[i].t < this.b && block_list[i].r <= this.l && max < block_list[i].r){
			ret = i;
			max = block_list[i].r;
		}
	}

	this.set(max, this.t);

	return ret;
}

/// Update position by tracing intersecting blocks towards right
Block.prototype.slipRight = function(ig, dest){
	var ret = block_list.length;
	var min = dest;

	for(var i = 0; i < block_list.length; i++){
		if(i !== ig && this.t < block_list[i].b && block_list[i].t < this.b && this.r <= block_list[i].l && block_list[i].l < min){
			ret = i;
			min = block_list[i].l;
		}
	}

	min -= this.r - this.l;
	this.set(min, this.t);

	return ret;
}

/// Initialize graphics objects associated with this block.
/// This is not done in the constructor because putting a block to the game
/// could fail.
Block.prototype.initGraphics = function(){
	var g = new createjs.Graphics();
	g.setStrokeStyle(1);
	g.beginStroke("#000000");
	g.beginFill("red");
	g.rect(0, 0, this.getW(), this.getH());
	var shape = new createjs.Shape(g);
	this.graphics = g;
	this.shape = shape;
	this.updateGraphics();
	blockStage.addChild(shape);
}

Block.prototype.update = function(){
	var oldbottom = this.b;
	var newbottom = Math.min(height, this.b + 2);
	this.slipDown(null, newbottom);

	// Only the newest block is controllable
	if(!gameOver && this === block_list[block_list.length-1]){
		if(keyState.left)
			this.slipLeft(-1, Math.max(0, this.l - 2));
		if(keyState.right)
			this.slipRight(-1, Math.min(width, this.r + 2));
		if(keyState.down)
			this.slipDown(-1, Math.min(this.b + downSpeed, height));
	}

	if(oldbottom !== this.b){
		this.updateGraphics();
		moved = true;
	}
}

Block.prototype.updateGraphics = function(){
	this.shape.x = this.l;
	this.shape.y = this.t;
}

var block_list = [];

function init(){
	window.addEventListener( 'keydown', onKeyDown, false );
	window.addEventListener( 'keyup', onKeyUp, false );
	requestAnimationFrame(animate);
}

function animate(timestamp) {
	moved = false;
	for(var i = 0; i < block_list.length; i++)
		block_list[i].update();
	if(!moved && !gameOver){
		/* if the top is above the dead line, game is over. */
		if(block_list.length && block_list[block_list.length-1].t < MAX_BLOCK_HEIGHT){
//			us.lastbreak = rand() % (NUM_BREAKTYPES + 1);
			gameOver = true;
			gameOverText.visible = gameOver;
/*			{int place;
			if(0 < (place = PutScore(gs.points)+1)){
				static char buf[64];
				us.mdelay = 5000;
				sprintf(buf, "HIGH SCORE %d%s place!!", place, place == 1 ? "st" : place == 2 ? "nd" : place == 3 ? "rd" : "th");
				us.message = buf;
			}}*/

			/* do a firework */
			if(clearall) for(var j = 0; j < block_list.length; j++){
				block_list[j].life = rs.nexti() % (MAX_DELAY * 5);
			}
		}
		else{
			spawnNextBlock();
		}
	}
	stage.update();

	requestAnimationFrame(animate);
}


/// Returns intersecting block with a given block.
/// It scans all blocks in the list.
/// @param p The block to examine collision with.
/// @param ig The block index to ignore collision detection.
/// @return Index of hit object or block_list.length if nothing hits
function getAt(p, ig){
	for(var i = 0; i < block_list.length; i++)
		if(i !== ig && p.l < block_list[i].r && block_list[i].l < p.r && p.t < block_list[i].b && block_list[i].t < p.b)
			return i;

	return block_list.length;
}


function spawnNextBlock(){
	var temp = new Block(0, 0, 0, 0);
	var retries = 0;
	do{
		if(100 < retries++)
			break;
		temp.r = nextx + (temp.l = rs.nexti() % (width - MIN_BLOCK_WIDTH - nextx));
		temp.b = nexty + (temp.t = rs.nexti() % (height / 4 - MIN_BLOCK_HEIGHT - nexty));
	} while(getAt(temp, -1) !== block_list.length);
	temp.initGraphics();
	block_list.push(temp);

	setNextBlock();

	return temp;
}

function setNextBlock(){
	nextx = rs.nexti() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH) + MIN_BLOCK_WIDTH;
	nexty = rs.nexti() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;
}


/// Spawn initial blocks.
function initBlocks(){
	var c = 10;
	var k = Math.floor((height - MAX_BLOCK_HEIGHT) * InitBlockRate);
	var cb = null; /* current block */
	if(block_list.length !== 0){
		cb = block_list[block_list.length-1];
		block_list.pop();
	}
	try{
		while(c--){
			var tmp = new Block(0,0,0,0);
			var retries = 0;
			do{
				if(100 < retries++)
					throw "Max retries reached";
				tmp.l = rs.nexti() % (width - MIN_BLOCK_WIDTH);
				tmp.t = height - k +
					rs.nexti() % (k - MIN_BLOCK_HEIGHT - MAX_BLOCK_HEIGHT);
				tmp.r = tmp.l + MIN_BLOCK_WIDTH + rs.nexti() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH);
				if(width < tmp.r) tmp.r = width;
				tmp.b = tmp.t + MIN_BLOCK_HEIGHT + rs.nexti() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT);
				if(height < tmp.b) tmp.b = height;
			} while(getAt(tmp, block_list.length) !== block_list.length && (!cb || !tmp.intersects(cb)));
			tmp.slipDown(null, height);
//			AddBlock(&bl, tmp.l, tmp.t, tmp.r, tmp.b);
			tmp.initGraphics();
			block_list.push(tmp);
		}
	}
	catch(e){
		console.write(e);
	}
	if(cb){
		cb.initGraphics();
		block_list.push(cb);
	}
	/*spawnNextBlock()*/;
}

function onKeyDown( event ) {
	// Annoying browser incompatibilities
	var code = event.which || event.keyCode;
	// Also support numpad plus and minus
	if(code === 37) // left
		keyState.left = true;
	if(code === 38) // up
		keyState.up = true;
	if(code === 39) // right
		keyState.right = true;
	if(code === 40) // down
		keyState.down = true;
}

function onKeyUp( event ) {
	// Annoying browser incompatibilities
	var code = event.which || event.keyCode;
	// Also support numpad plus and minus
	if(code === 37) // left
		keyState.left = false;
	if(code === 38) // up
		keyState.up = false;
	if(code === 39) // right
		keyState.right = false;
	if(code === 40) // down
		keyState.down = false;
}

window.onload = function(){
	canvas = document.getElementById("stage");
	canvas.oncontextmenu = function(){return false;};
	width = parseInt(canvas.style.width);
	height = parseInt(canvas.style.height);

	stage = new createjs.Stage(canvas);
	stage.enableMouseOver();

	blockStage = new createjs.Container();
	stage.addChild(blockStage);
	overlay = new createjs.Container();
	stage.addChild(overlay);

	gameOverText = new createjs.Text("GAME OVER", "bold 40px Arial", "#007f7f");
	gameOverText.visible = false;
	overlay.addChild(gameOverText);
	var bounds = gameOverText.getBounds();
	gameOverText.x = width / 2 - bounds.width / 2;
	gameOverText.y = height / 2 - bounds.height / 2;

	setNextBlock();
	initBlocks();

	init();
}

})();
