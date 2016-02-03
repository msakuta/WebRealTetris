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
var MIN_SPACE;
var ALT_SPACE;
var InitBlockRate = 0.5;
var downSpeed = 20;
var ClearRate = 0.15;
var AlertRate = 0.3;

function Block(l, t, r, b){
	this.l = l;
	this.t = t;
	this.r = r;
	this.b = b; /* dimension of the block. */
//	fixed vx, vy; /* velocity in fixed format. display is in integral coordinates system, anyway. */
	this.life = 0; /* if not UINT_MAX, the block is to be removed after this frames. */
}

/// Pseudo destructor that removes the shape from the canvas.
Block.prototype.destroy = function(){
	if(this.shape){
		blockStage.removeChild(this.shape);
	}
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

Block.prototype.rotate = function(){
	this.toggleUp = true;
	var pb = block_list[block_list.length-1];
	var newblock = new Block(0,0,0,0);
	newblock.l = (this.t - this.b + this.l + this.r) / 2; /* rotation math */
	newblock.t = (this.l - this.r + this.t + this.b) / 2;
	newblock.r = (this.b - this.t + this.l + this.r) / 2;
	newblock.b = (this.r - this.l + this.t + this.b) / 2;
	var i = getAt(newblock, -1);
	if(i !== block_list.length-1){ /* some blocks got in the way */
//					var pb = block_list[i];
//					AddTefbox(g_ptefl, tb.l, tb.t, tb.r, tb.b, RGB(64, 64, 16), WG_BLACK, .5, 0);
//					AddTefbox(g_ptefl, MAX(pb->l, tb.l), MAX(pb->t, tb.t), MIN(pb->r, tb.r),
//						MIN(pb->b, tb.b), RGB(192, 192, 32), WG_BLACK, .75, 0);
	}
	else{
		// If a part of rotated block go outside of the stage, force
		// the position to be inside it.
		if(newblock.l < 0)
			newblock.set(0, newblock.t);
		if(width < newblock.r){
			newblock.l -= newblock.r - width;
			newblock.r = width;
		}
//					AddTefbox(g_ptefl, pb->l, pb->t, pb->r, pb->b, RGB(0, 128, 64), WG_BLACK, 1., TEF_BORDER);
		this.l = newblock.l;
		this.t = newblock.t;
		this.r = newblock.r;
		this.b = newblock.b;
		this.updateGraphics(true);
	}
}

Block.prototype.updateGraphics = function(rotated){
	if(rotated){
		var g = this.graphics;
		g.clear();
		g.setStrokeStyle(1);
		g.beginStroke("#000000");
		g.beginFill("red");
		g.rect(0, 0, this.getW(), this.getH());
	}
	this.shape.x = this.l;
	this.shape.y = this.t;
}

var block_list = [];

function init(){
	window.addEventListener( 'keydown', onKeyDown, false );
	window.addEventListener( 'keyup', onKeyUp, false );
	createjs.Ticker.setFPS(30);
	createjs.Ticker.addEventListener("tick", animate);
}

function animate(timestamp) {
	moved = false;
	for(var i = 0; i < block_list.length; i++)
		block_list[i].update();

	// Perform collapse check only if everything is settled because this check
	// is heavy.
	if(!moved)
		collapseCheck();

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
}


function collapseCheck(){
	for(var i = 0; i < block_list.length; i++){
		/* check horizontal spaces */
		var spaceb = width;
		var spacet = width;
		for(var j = 0; j+1 < block_list.length; j++){
			if(i === j) continue;
			/* bottom line */
			if(block_list[j].t < block_list[i].b && block_list[i].b <= block_list[j].b)
				spaceb -= block_list[j].r - block_list[j].l;
			/* top line */
			if(block_list[j].t <= block_list[i].t && block_list[i].t < block_list[j].b)
				spacet -= block_list[j].r - block_list[j].l;
		}
		if(spaceb < MIN_SPACE || spacet < MIN_SPACE){
			var yb = block_list[i].b;
			var yt = block_list[i].t;
	//			us.lastbreak = 1 + rand() % NUM_BREAKTYPES;
			/* destroy myself. */
			block_list[i].destroy();
			block_list.splice(i, 1);
	//			block_list[i].life = rand() % MAX_DELAY;
			/* destroy all blocks in a horizontal line. */
			if(spaceb < MIN_SPACE)
			for(j = 0; j < block_list.length; j++){
				if(block_list[j].t < yb && yb <= block_list[j].b){
	//					block_list[j].life = rand() % MAX_DELAY;
					block_list[j].destroy();
					block_list.splice(j, 1);
	/*					BlockBreak(&p->l[j]);
					KillBlock(p, j);
					points++;*/
				}
			}
			if(spacet < MIN_SPACE)
			for(j = 0; j < block_list.length; j++){
				if(block_list[j].t <= yt && yt < block_list[j].b){
					//block_list[j].life = rand() % MAX_DELAY;
					block_list[j].destroy();
					block_list.splice(j, 1);
	/*					BlockBreak(&p->l[j]);
					KillBlock(p, j);
					points++;*/
				}
			}
			moved = true;
		}
	}
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
	if(code === 38){ // up
		keyState.up = true;
		if(0 < block_list.length)
		 	block_list[block_list.length-1].rotate();
	}
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

	// These parameters must be initialized after width and height are determined
	MIN_SPACE = width * ClearRate;
	ALT_SPACE = width * AlertRate;

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
