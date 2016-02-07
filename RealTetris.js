var RealTetris = new (function(){
'use strict';
var scope = this;
var canvas;
var width;
var height;
var minimapCanvas;
var stage;
var underlay;
var overlay;
var blockStage;
var moved = false;
var nextBlock;
var gameOver = false;
var paused = false;
var clearall = true;
var gameOverText = null;
var pauseText = null;
var keyState = {
	left: false,
	up: false,
	right: false,
	down: false,
};
var score = 0;
var highScores = [];
var rs = new Xor128();

var MAX_BLOCK_WIDTH = 50;
var MAX_BLOCK_HEIGHT = 50;
var MIN_BLOCK_WIDTH = 10;
var MIN_BLOCK_HEIGHT = 10;
var MAX_DELAY = 25; /* maximum block destroying delay */
var InitBlockRate = 0.5;
var downSpeed = 20;
var AlertRate = 0.3;
var BLOCK_ERASE_SCORE = 100;

//============== Vector arithmetics ======================
// These functions are meant to be used for two-element arrays.
function vecslen(v){
	return v[0] * v[0] + v[1] * v[1];
}

function veclen(v){
	return Math.sqrt(vecslen(v));
}

function vecnorm(v){
	var len = veclen(v);
	return [v[0] / len, v[1] / len];
}

function vecscale(v,s){
	return [v[0] * s, v[1] * s];
}

function vecadd(v1,v2){
	return [v1[0] + v2[0], v1[1] + v2[1]];
}

function vecsub(v1,v2){
	return [v1[0] - v2[0], v1[1] - v2[1]];
}

function vecdot(v1,v2){
	return v1[0] * v2[0] + v1[1] * v2[1];
}

function veccross(v1,v2){
	return v1[0] * v2[1] - v1[1] * v2[0];
}


//============== Block class definition ======================
function Block(l, t, r, b){
	this.l = l;
	this.t = t;
	this.r = r;
	this.b = b; /* dimension of the block. */
	this.subblocks = null;
	this.life = -1; /* if not -1, the block is to be removed after this frames. */
	this.vx = 0; // Velocity along horizontal axis
	this.eraseHint = 0; // Hint color fraction indicating how this block is close to collapse
}

/// Pseudo destructor that removes the shape from the canvas.
Block.prototype.destroy = function(){
	if(this.shape){
		blockStage.removeChild(this.shape);
	}
	if(this.underlayShape)
		underlay.removeChild(this.underlayShape);
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
	var changed = this.l !== x || this.t !== y;
	this.r -= this.l - x;
	this.l = x;
	this.b -= this.t - y;
	this.t = y;
	if(changed)
		this.updateCache();
};

Block.prototype.updateCache = function(){
	if(!this.subblocks)
		return;
	this.cachedSubBlocks = [];
	for(var i = 0; i < this.subblocks.length; i++){
		var pseudoBlock = new Block(
			this.l + this.subblocks[i].l,
			this.t + this.subblocks[i].t,
			this.l + this.subblocks[i].r,
			this.t + this.subblocks[i].b);
		this.cachedSubBlocks.push(pseudoBlock);
	}
}

/// Enumerate all static blocks including subblocks
/// @returns if the enumeration is aborted by callback function's returned value
function enumSubBlocks(callback, ignore){
	for(var i = 0; i < block_list.length; i++){
		var block = block_list[i];
		if(block === ignore)
			continue;
		if(block.enumSubBlocks(callback))
			return true;
	}
	return false;
}

/// Enumerate the block and subblocks in a consistent way.
/// subblocks are transformed to global coordinates and passed as pseudo block.
/// @returns if the enumeration is aborted by callback function's returned value
Block.prototype.enumSubBlocks = function(callback){
	if(this.subblocks){
		if(!this.cachedSubBlocks)
			this.updateCache();
		for(var j = 0; j < this.cachedSubBlocks.length; j++){
			if(callback(this.cachedSubBlocks[j], this))
				return true;
		}
	}
	else {
		if(callback(this, this))
			return true;
	}
	return false;
}

/// Update position by tracing intersecting blocks downwards
Block.prototype.slipDown = function(ig, dest){
	var ret = block_list.length;
	var min = dest;
	var scope = this;

	enumSubBlocks(function(block){
		if(block === ig)
			return;
		scope.enumSubBlocks(function(sb){
			if(sb.l < block.r && block.l < sb.r && sb.b <= block.t && block.t < min - scope.b + sb.b){
				ret = block;
				min = block.t + scope.b - sb.b;
			}
		});
	}, this);

	min -= this.b - this.t;
	this.set(this.l, min);

	return ret;
};

/// Update position by tracing intersecting blocks towards left
Block.prototype.slipLeft = function(ig, dest){
	var ret = block_list.length;
	var max = dest;
	var scope = this;

	enumSubBlocks(function(block){
		if(block === ig)
			return;
		scope.enumSubBlocks(function(sb){
			if(sb.t < block.b && block.t < sb.b && block.r <= sb.l && max - scope.l + sb.l < block.r){
				ret = block;
				max = block.r + scope.l - sb.l;
			}
		});
	}, this);

	this.set(max, this.t);

	return ret;
}

/// Update position by tracing intersecting blocks towards right
Block.prototype.slipRight = function(ig, dest){
	var ret = block_list.length;
	var min = dest;
	var scope = this;

	enumSubBlocks(function(block){
		if(block === ig)
		 	return;
		scope.enumSubBlocks(function(sb){
			if(sb.t < block.b && block.t < sb.b && sb.r <= block.l && block.l < min - scope.r + sb.r){
				ret = block;
				min = block.l + scope.r - sb.r;
			}
		});
	}, this);

	min -= this.r - this.l;
	this.set(min, this.t);

	return ret;
}

/// Initialize graphics objects associated with this block.
/// This is not done in the constructor because putting a block to the game
/// could fail.
Block.prototype.initGraphics = function(){
	var g = new createjs.Graphics();
	var shape = new createjs.Shape(g);
	this.graphics = g;
	this.shape = shape;
	blockStage.addChild(shape);
	if(controlled === this){
		g = new createjs.Graphics();
		shape = new createjs.Shape(g);
		this.underlayGraphics = g;
		this.underlayShape = shape;
		underlay.addChild(shape);
	}
	this.updateGraphics(true);
}

Block.prototype.update = function(){
	var oldbottom = this.b;
	var newbottom = Math.min(height, this.b + 2);
	this.slipDown(null, newbottom);

	// Only the newest block is controllable
	if(!gameOver && this === controlled){
		if(keyState.left)
			this.vx = Math.max(-10, this.vx - 0.5);
		if(keyState.right)
			this.vx = Math.min(10, this.vx + 0.5);

		// Decelerate if neither left or right key is pressed
		if(!keyState.left && !keyState.right){
			if(Math.abs(this.vx) < 1)
				this.vx = 0;
			else
				this.vx -= (this.vx < 0 ? -1 : 1);
		}

		// this.vx can be fractional, but we disallow fractional position
		// because it's hard to see the difference.
		if(this.vx < 0){
			this.slipLeft(-1, Math.round(Math.max(0, this.l + this.vx)));
			if(this.l === 0)
				this.vx = 0;
		}
		else if(0 < this.vx){
			this.slipRight(-1, Math.round(Math.min(width, this.r + this.vx)));
			if(this.r === width)
				this.vx = 0;
		}

		if(keyState.down){
			this.slipDown(-1, Math.min(this.b + downSpeed, height));
			score += 1;
		}
	}

	if(oldbottom !== this.b){
		this.updateGraphics();
		moved = true;
	}
	else if(controlled === this){
		// Release control of this block and stop lateral motion.
		controlled = null;
		this.vx = 0;
		this.updateGraphics(true);
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
	var newsubblocks;
	if(this.subblocks){
		// Rotate subblocks counterclockwise.
		var cx = this.getW();
		newsubblocks = [];
		for(var i = 0; i < this.subblocks.length; i++){
			var subblock = this.subblocks[i];
			var newsubblock = {
				l: subblock.t,
				t: -subblock.r + cx,
				r: subblock.b,
				b: -subblock.l + cx,
			};
			newsubblocks.push(newsubblock);
		}
	}
	var i = getAt(newblock, block_list.indexOf(this));
	if(i !== block_list.length){ /* some blocks got in the way */
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
		this.subblocks = newsubblocks;
		this.updateGraphics(true);
	}
}

Block.prototype.updateGraphics = function(rotated){
	/// An internal class that represents an edge of rectangle.
	/// Used for outlining compound blocks.
	function Edge(base, dir){
		this.base = base;
		this.dir = dir;
	}
	Edge.prototype.getEnd = function(){
		return vecadd(this.base, this.dir);
	}

	if(!this.graphics || !this.shape)
		return;
	if(rotated){
		var g = this.graphics;

		// Set color tone by control state and erase hint
		var color;
		if(controlled === this)
			color = "#00ffaf";
		else
			color = "rgb(" + (this.eraseHint * 255).toFixed() + ", 0, 191)";

		g.clear();
		if(this.subblocks){
			for(var i = 0; i < this.subblocks.length; i++){
				g.setStrokeStyle(1);
				g.beginFill(color);
				g.rect(this.subblocks[i].l, this.subblocks[i].t, this.subblocks[i].r - this.subblocks[i].l, this.subblocks[i].b - this.subblocks[i].t);
			}

			// Enumerate edges of subblocks to trace union of the edges in this
			// block.
			var edges = [];
			for(var i = 0; i < this.subblocks.length; i++){
				var sb = this.subblocks[i];
				edges.push(new Edge([sb.l, sb.t], [0, sb.b - sb.t]));
				edges.push(new Edge([sb.l, sb.b], [sb.r - sb.l, 0]));
				edges.push(new Edge([sb.r, sb.b], [0, sb.t - sb.b]));
				edges.push(new Edge([sb.r, sb.t], [sb.l - sb.r, 0]));
			}
			g.setStrokeStyle(1);
			g.beginStroke("#000000");
			for(var i = 0; i < edges.length; i++){
				var start = edges[i].base;
				var end = edges[i].getEnd();
				var skip = false;
				for(var j = 0; j < edges.length; j++){
					if(i === j)
						continue;
					var ndir = vecnorm(edges[i].dir);
					var normal = [ndir[1], -ndir[0]]; // Vector normal to the direction

					// There could be floating point error in vector aritmetics, so
					// directly comparing values could yield unexpected result,
					// but the coordinates are all integer.
					if(veccross(edges[i].dir, edges[j].dir) === 0 && vecdot(edges[i].base, normal) === vecdot(edges[j].base, normal)){
						var iStart = vecdot(start, ndir);
						var iEnd = vecdot(end, ndir);
						var jStart = vecdot(edges[j].base, ndir);
						var jEnd = vecdot(edges[j].getEnd(), ndir);
						var jDots = [jStart, jEnd];
						var jPoints = [edges[j].base, edges[j].getEnd()];

						// Sort the two arrays in ascending order.
						// The edge from the outer loop (i) has always iEnd greater
						// than iStart, but one from the inner loop (j) not
						// necessarily does, so we need to sort the end points in
						// consistent order to make the algorithms work.
						// Array.sort() could not be used because the two arrays
						// should be synchronized.
						if(jEnd < jStart){
							jDots.reverse();
							jPoints.reverse();
						}

						// If the whole edge is contained in another edge,
						// No parts of it should be rendered.
						if(jDots[0] <= iStart && iEnd <= jDots[1])
							skip = true;
						else if(iStart <= jDots[0] && jDots[0] < iEnd)
							end = jPoints[0];
						else if(iStart <= jDots[1] && jDots[1] < iEnd)
							start = jPoints[1];
					}
				}
				if(!skip)
					g.moveTo(start[0], start[1]).lineTo(end[0], end[1]);
			}
		}
		else{
			g.setStrokeStyle(1);
			g.beginStroke("#000000");
			g.beginFill(color);
			g.rect(0, 0, this.getW(), this.getH());
		}

		if(controlled === this){
			// Draw fall path guide
			g = this.underlayGraphics;
			g.clear();
			g.beginStroke("#3f7f7f");
			if(this.subblocks){
				for(var i = 0; i < this.subblocks.length; i++){
					var sb = this.subblocks[i];
					g.mt(sb.l, sb.b).lt(sb.l, height);
					g.mt(sb.r, sb.b).lt(sb.r, height);
				}
			}
			else{
				g.mt(0, this.getH()).lt(0, height);
				g.mt(this.getW(), this.getH()).lt(this.getW(), height);
			}
		}
	}
	this.shape.x = this.l;
	this.shape.y = this.t;

	// Update the shape in underlay
	if(controlled === this){
		this.underlayShape.x = this.l;
		this.underlayShape.y = this.t;
	}
	else{
		// Delete guiding lines if it's no longer controlled
		if(this.underlayShape){
			underlay.removeChild(this.underlayShape);
			delete this.underlayShape;
		}
		if(this.underlayGraphics)
			delete this.underlayGraphics;
	}
}

Block.prototype.addSubBlock = function(l, t, r, b){
	if(!this.subblocks)
		this.subblocks = [];
	this.subblocks.push({l:l, t:t, r:r, b:b});

	for(var i = 0; i < this.subblocks.length; i++){
		var sb = this.subblocks[i];
		// Don't add subblocks with negative coordinates because it will violate
		// the convention of bounding rectangle!
//		if(sb.l < 0)
//			this.l -= sb.l;
//		if(sb.t < 0)
//			this.t -= sb.t;

		if(this.r - this.l < sb.r)
			this.r = this.l + sb.r;
		if(this.b - this.t < sb.b)
			this.b = this.t + sb.b;
	}
};

var block_list = [];
var controlled = null;

function init(){
	window.addEventListener( 'keydown', onKeyDown, false );
	window.addEventListener( 'keyup', onKeyUp, false );
	createjs.Ticker.setFPS(30);
	createjs.Ticker.addEventListener("tick", animate);
}

function animate(timestamp) {
	if(paused){
		stage.update();
		return;
	}
	moved = false;
	for(var i = 0; i < block_list.length;){
		block_list[i].update();
		if(block_list[i].life === 0){
			block_list[i].destroy();
			block_list.splice(i, 1);
		}
		else{
			i++;
		}
	}

	// Perform collapse check only if everything is settled because this check
	// is heavy.
	if(!moved && !gameOver)
		collapseCheck();

	if(!moved && !gameOver){
		/* if the top is above the dead line, game is over. */
		if(block_list.length && block_list[block_list.length-1].t < MAX_BLOCK_HEIGHT){
//			us.lastbreak = rand() % (NUM_BREAKTYPES + 1);
			gameOver = true;
			gameOverText.visible = gameOver;

			// Update the high scores
			var insertionIdx = highScores.length;
			for(var j = 0; j < highScores.length; j++){
				if(highScores[j].score < score){
					insertionIdx = j;
					break;
				}
			}
			highScores.splice(insertionIdx, 0, {score: score, date: new Date()});
			// Limit the number of high scores
			if(20 < highScores.length)
				highScores.pop();

			if(typeof(Storage) !== "undefined"){
				localStorage.setItem("RealTetris", serialize());
			}
			updateHighScores();

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

	var scoreElem = document.getElementById("score");
	if(scoreElem)
		scoreElem.innerHTML = score;
}


function collapseCheck(){
	// The minimum space becomes half when you get 30000 points and increasingly
	// harder as the score goes up.
	// Let's leave at least 1% of width in even the hardest condition, or it
	// will be really impossible.
	var MIN_SPACE = width * (0.01 + 0.29 * 30000 / (score + 30000));
	var ALT_SPACE = MIN_SPACE + AlertRate * width;

	// Clear the hint variable because we'll have max with other block's values
	for(var i = 0; i < block_list.length; i++)
		block_list[i].eraseHint = 0;

	for(var i = 0; i < block_list.length; i++){
		var block = block_list[i];
		block.enumSubBlocks(function(sb){
			/* check horizontal spaces */
			var spaceb = width;
			var spacet = width;

			/// Enumerate all the other blocks to determine space
			enumSubBlocks(function(block2){
				/* bottom line */
				if(block2.t < sb.b && sb.b <= block2.b)
					spaceb -= block2.r - block2.l;
				/* top line */
				if(block2.t <= sb.t && sb.t < block2.b)
					spacet -= block2.r - block2.l;
			}, block);

			var yb = sb.b;
			var yt = sb.t;
			if(spaceb < MIN_SPACE || spacet < MIN_SPACE){
		//			us.lastbreak = 1 + rand() % NUM_BREAKTYPES;
				/* destroy myself. */
				block.life = 0;
				score += BLOCK_ERASE_SCORE;
		//			block_list[i].life = rand() % MAX_DELAY;
				/* destroy all blocks in a horizontal line. */
				if(spaceb < MIN_SPACE)
				for(var j = 0; j < block_list.length; j++){
					if(block_list[j].t < yb && yb <= block_list[j].b){
		//					block_list[j].life = rand() % MAX_DELAY;
						block_list[j].life = 0;
						score += BLOCK_ERASE_SCORE;
					}
				}
				if(spacet < MIN_SPACE)
				for(var j = 0; j < block_list.length; j++){
					if(block_list[j].t <= yt && yt < block_list[j].b){
						//block_list[j].life = rand() % MAX_DELAY;
						block_list[j].life = 0;
						score += BLOCK_ERASE_SCORE;
					}
				}
				moved = true;
			}
			else{
				// Show blocks near erasing threshold with red color tone gradually.
				// Only blocks with gaps less than ALT_SPACE are colored.
				var eraset = Math.min(1, Math.max(0, (ALT_SPACE - spacet) / (ALT_SPACE - MIN_SPACE)));
				var eraseb = Math.min(1, Math.max(0, (ALT_SPACE - spaceb) / (ALT_SPACE - MIN_SPACE)));
				block.eraseHint = Math.max(eraset, eraseb);
				block.updateGraphics(true);
				enumSubBlocks(function(block2, block2Parent){
					var changed = false;
					if(block2.t <= yt && yt < block2.b && block2.eraseHint < eraset){
						changed = true;
						block2Parent.eraseHint = eraset;
					}
					if(block2.t <= yb && yb < block.b && block2.eraseHint < eraseb){
						changed = true;
						block2Parent.eraseHint = eraseb;
					}
					if(changed)
						block2Parent.updateGraphics(true);
				}, block);
			}
		});
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
	var retries = 0;
	do{
		if(100 < retries++)
			break;
		nextBlock.set(rs.nexti() % (width - MIN_BLOCK_WIDTH - nextBlock.getW()),
			rs.nexti() % (height / 4 - MIN_BLOCK_HEIGHT - nextBlock.getH()));
	} while(getAt(nextBlock, -1) !== block_list.length);
	controlled = nextBlock;
	nextBlock.initGraphics();
	block_list.push(nextBlock);

	setNextBlock();

	return nextBlock;
}

function setNextBlock(){
	var type = rs.nexti() % 4;
	// type === 0: Simple block
	//  +-----+
	//  |     |
	//  |     |
	//  +-----+
	//
	// type === 1: Angled block
	//  +-----+
	//  |  +--+
	//  |  |
	//  +--+
	//
	// type === 2: T-shape
	//  +--+
	//  |  +--+
	//  |  +--+
	//  +--+
	//
	// type === 3: Z-shape
	//     +--+
	//  +--+  |
	//  |  +--+
	//  +--+
	//
	// type === 4: S-shape
	//  +--+
	//  |  +--+
	//  +--+  |
	//     +--+
	//
	if(type === 0){
		var nextx = rs.nexti() % (MAX_BLOCK_WIDTH - MIN_BLOCK_WIDTH) + MIN_BLOCK_WIDTH;
		var nexty = rs.nexti() % (MAX_BLOCK_HEIGHT - MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;
		var posx = rs.nexti() % (width - MIN_BLOCK_WIDTH - nextx);
		var posy = rs.nexti() % (height / 4 - MIN_BLOCK_HEIGHT - nexty);
		nextBlock = new Block(posx, posy, posx + nextx, posy + nexty);
	}
	else{
		var minWidth = MIN_BLOCK_WIDTH * 2 + 1;
		var minHeight = MIN_BLOCK_HEIGHT * (type === 1 ? 2 : 3) + 1;
		var nextx = rs.nexti() % (MAX_BLOCK_WIDTH - minWidth) + minWidth;
		var nexty = rs.nexti() % (MAX_BLOCK_HEIGHT - minHeight) + minHeight;
		var posx = rs.nexti() % (width - MIN_BLOCK_WIDTH - nextx);
		var posy = rs.nexti() % (height / 4 - MIN_BLOCK_HEIGHT - nexty);
		nextBlock = new Block(posx, posy, posx + nextx, posy + nexty);
		nextBlock.subblocks = [];
		var hdiv = rs.nexti() % (nextx - 2 * MIN_BLOCK_WIDTH) + MIN_BLOCK_WIDTH;
		var y0 = 0;
		var y1 = nexty;

		if(type === 3)
			y0 = rs.nexti() % (nexty - 3 * MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;
		if(type === 4)
			y1 = rs.nexti() % (nexty - 3 * MIN_BLOCK_HEIGHT) + 2 * MIN_BLOCK_HEIGHT + 1;
		nextBlock.addSubBlock(0, y0, hdiv, y1);

		if(type === 2){
			var y = rs.nexti() % (nexty - 3 * MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;
			nextBlock.addSubBlock(hdiv, y, nextBlock.getW(), y + rs.nexti() % (nexty - y - 2 * MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT);
		}
		else if(type === 3)
			nextBlock.addSubBlock(hdiv, 0, nextBlock.getW(), rs.nexti() % (nexty - y0 - 2 * MIN_BLOCK_HEIGHT) + y0 + MIN_BLOCK_HEIGHT);
		else if(type === 4){
			y0 = rs.nexti() % (y1 - 2 * MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT;
			nextBlock.addSubBlock(hdiv, y0, nextBlock.getW(), nextBlock.getH());
		}
		else
			nextBlock.addSubBlock(hdiv, 0, nextBlock.getW(), rs.nexti() % (nexty - 2 * MIN_BLOCK_HEIGHT) + MIN_BLOCK_HEIGHT);
		var angle = rs.nexti() % 3;
		for(var i = 0; i < angle; i++)
			nextBlock.rotate();
	}
}


/// Spawn initial blocks.
function initBlocks(){
	var c = 30;
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
		console.log(e);
	}
	if(cb){
		cb.initGraphics();
		block_list.push(cb);
	}
	/*spawnNextBlock()*/;
}

function onKeyDown( event ) {
	if(event.keyCode == 80){ // 'p'
		scope.pause();
	}

	// Ignore key inputs when paused
	if(paused)
		return;

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

	// Prevent arrow keys from scrolling the screen
	if(37 <= code && code <= 40)
		event.preventDefault();
}

function onKeyUp( event ) {
	// Ignore key inputs when paused
	if(paused)
		return;

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

this.pause = function(){
	paused = !paused;
	pauseText.visible = paused;
}

function deserialize(stream){
	var data = JSON.parse(stream);
	if(data !== null){
		for(var i = 0; i < data.highScores.length; i++)
			highScores.push({score: data.highScores[i].score, date: new Date(data.highScores[i].date)});
	}
}

function serialize(){
	var jsonHighScores = [];
	for(var i = 0; i < highScores.length; i++)
		jsonHighScores.push({score: highScores[i].score, date: highScores[i].date.toJSON()});
	var saveData = {highScores: jsonHighScores};
	return JSON.stringify(saveData);
}

/// Update current high score ranking to the table.
function updateHighScores(){
	var elem = document.getElementById("highScores");
	if(highScores.length === 0){
		// Clear the table if no high scores are available
		elem.innerHTML = "";
		return;
	}
	// Table and its child nodes could be elements created by document.createElement(),
	// but this string concatenation is far simpler.
	var table = "High Scores:<table><tr><th>Place</th><th>Date</th><th>Score</th></tr>";
	for(var i = 0; i < highScores.length; i++){
		table += "<tr><td>" + (i+1) + "</td><td>" + highScores[i].date.toLocaleString() + "</td><td>" + highScores[i].score + "</td></tr>";
	}
	table += "</table>";
	elem.innerHTML = table;
}

window.onload = function(){
	canvas = document.getElementById("stage");
	canvas.oncontextmenu = function(){return false;};
	width = parseInt(canvas.style.width);
	height = parseInt(canvas.style.height);

	stage = new createjs.Stage(canvas);
	stage.enableMouseOver();

	underlay = new createjs.Container();
	stage.addChild(underlay);
	blockStage = new createjs.Container();
	stage.addChild(blockStage);
	overlay = new createjs.Container();
	stage.addChild(overlay);

	gameOverText = new createjs.Text("GAME OVER", "bold 40px Arial", "#ff3f3f");
	gameOverText.visible = false;
	overlay.addChild(gameOverText);
	var bounds = gameOverText.getBounds();
	gameOverText.x = width / 2 - bounds.width / 2;
	gameOverText.y = height / 2 - bounds.height / 2;

	pauseText = new createjs.Text("PAUSED", "bold 40px Arial", "#7fff00");
	pauseText.visible = false;
	overlay.addChild(pauseText);
	var bounds = pauseText.getBounds();
	pauseText.x = width / 2 - bounds.width / 2;
	pauseText.y = gameOverText.y + bounds.height;

	if(typeof(Storage) !== "undefined"){
		try{
			deserialize(localStorage.getItem("RealTetris"));
		}
		catch(e){
			// If something got wrong about the storage, clear everything.
			// Doing so would guarantee the problem no longer persists, or we
			// would be caught in the same problem repeatedly.
			highScores = [];
			localStorage.removeItem("RealTetris");
		}
		updateHighScores();
	}

	scope.restart();
}

this.restart = function(){
	for(var i = 0; i < block_list.length; i++){
		block_list[i].destroy();
	}
	block_list = [];
	gameOver = false;
	gameOverText.visible = false;
	score = 0;
	setNextBlock();
	initBlocks();

	init();
}

})();
