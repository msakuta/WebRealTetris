
var canvas;
var width;
var height;
var minimapCanvas;
var stage;
var shape;

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

Block.prototype.update = function(){
	var newtop = Math.min(height - this.getH(), this.t + 2);
	this.b += newtop - this.t;
	this.t = newtop;
	this.updateGraphics();
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
	for(var i = 0; i < block_list.length; i++)
		block_list[i].update();
	stage.update();

	requestAnimationFrame(animate);
}

window.onload = function(){
	canvas = document.getElementById("stage");
	canvas.oncontextmenu = function(){return false;};
	width = parseInt(canvas.style.width);
	height = parseInt(canvas.style.height);

	var rs = new Xor128();

	stage = new createjs.Stage(canvas);
	stage.enableMouseOver();

	for(var i = 0; i < 10; i++){
		var w = rs.nexti() % 100;
		var h = rs.nexti() % 100;
		var l = rs.nexti() % (canvas.width - w);
		var t = rs.nexti() % (canvas.height - h);
		var block = new Block(l, t, l + w, t + h);
		block_list.push(block);

		var g = new createjs.Graphics();
		g.setStrokeStyle(1);
		g.beginStroke("#000000");
		g.beginFill("red");
		g.rect(0, 0, block.getW(), block.getH());
		var shape = new createjs.Shape(g);
		block.graphics = g;
		block.shape = shape;
		block.updateGraphics();
		stage.addChild(shape);
	}

	init();
}
