'use strict';
var select = document.createElement('select');
select.id = 'color-scheme-select';
let styles = {};
function addOption(option) {
	var optionEl = document.createElement('option');
	select.appendChild(optionEl);
	optionEl.value = option;
	optionEl.appendChild(document.createTextNode(option));
	styles[option] = document.getElementById('style-' + option).textContent;
}
function setStyle(style) {
	document.querySelector('style').textContent = styles[style];
}
addOption('dark');
addOption('light');
document.body.appendChild(select);
if (window.localStorage && 
	localStorage.getItem('ted-color-scheme')) {
	setStyle(localStorage.getItem('ted-color-scheme'));
}

select.onchange = function () {
	var chosen = select.value;
	if (window.localStorage) {
		localStorage.setItem('ted-color-scheme', chosen);
	}
	setStyle(chosen);
};
