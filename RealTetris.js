'use strict';

var canvas;
var width;
var height;
var minimapCanvas;
var stage;
var moved = false;
var nextx;
var nexty;
var rs = new Xor128();

var MAX_BLOCK_WIDTH = 50;
var MAX_BLOCK_HEIGHT = 50;
var MIN_BLOCK_WIDTH = 10;
var MIN_BLOCK_HEIGHT = 10;
var InitBlockRate = 0.5;

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
	stage.addChild(shape);
}

Block.prototype.update = function(){
	var oldbottom = this.b;
	var newbottom = Math.min(height, this.b + 2);
	this.slipDown(null, newbottom);
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
	requestAnimationFrame(animate);
}

function animate(timestamp) {
	moved = false;
	for(var i = 0; i < block_list.length; i++)
		block_list[i].update();
	if(!moved){
		spawnNextBlock();
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

window.onload = function(){
	canvas = document.getElementById("stage");
	canvas.oncontextmenu = function(){return false;};
	width = parseInt(canvas.style.width);
	height = parseInt(canvas.style.height);

	stage = new createjs.Stage(canvas);
	stage.enableMouseOver();

	setNextBlock();
	initBlocks();

	init();
}
