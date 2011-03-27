
function getShortLicenseName(license) {
	license = license.replace("GNU General Public License", "GPL");
	license = license.replace("Creative Commons", "CC");
	license = license.replace("Attribution", "by");
	license = license.replace("Share Alike", "SA");
	return license;
}

function isNonFreeLicense(license) {
	return license == "UNKNOWN"
		|| license == "Creative Commons"	// ambiguous
		|| license == "Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"
		|| license == "Creative Commons Sampling Plus 1.0";
}


function updateLicensePieData(chartId, legendId, licensePieData) {
	dojo.require("dojox.charting.Chart2D");
	dojo.require("dojox.charting.plot2d.Pie");
	dojo.require("dojox.charting.action2d.Highlight");
	dojo.require("dojox.charting.action2d.MoveSlice");
	dojo.require("dojox.charting.action2d.Tooltip");
	dojo.require("dojox.charting.themes.MiamiNice");
	dojo.require("dojox.charting.widget.Legend");

	if (licensePieData == null)
		return;

	var getTooltip = function(license, fileCount, fileSum) {
		return license + " (" + fileCount + " files, " + (Math.round((fileCount / fileSum) * 1000.0) / 10.0) + "%)";
	}

	var sum = 0;
	for (var i in licensePieData) {
		sum += licensePieData[i];
	}

	var data = [];
	var other = 0;
	var otherTooltip = "";
	for (var i in licensePieData) {
		var percent = licensePieData[i] / sum;
		if (percent < 0.05 && i != "UNKNOWN") {
			other += licensePieData[i];
			otherTooltip += getTooltip(i, licensePieData[i], sum) + "<br/" + ">";
		} else {
			data.push({
				y: licensePieData[i],
				text: getShortLicenseName(i),
				tooltip: getTooltip(i, licensePieData[i], sum)
			});
		}
	}
	data.push({
		y: other,
		text: "Other",
		tooltip: "<b>" + getTooltip("Other", other, sum) + "</b>" + "<br/" + ">" + otherTooltip
	});

	dojo.addOnLoad(function() {
		var licensePie = null;
		var node = dojo.byId(chartId);
		if (node.widget == null) {
			licensePie = new dojox.charting.Chart2D(chartId);
			licensePie.setTheme(dojox.charting.themes.MiamiNice).addPlot("default", {
				type: "Pie",
				font: "normal normal 11pt Tahoma",
				fontColor: "black",
				labelOffset: -30,
				radius: 80
			});
			var anim_a = new dojox.charting.action2d.MoveSlice(licensePie, "default");
			var anim_b = new dojox.charting.action2d.Highlight(licensePie, "default");
			var anim_c = new dojox.charting.action2d.Tooltip(licensePie, "default");
			node.widget = licensePie;
		} else {
			licensePie = node.widget;
		}
		licensePie.removeSeries("licenses");
		licensePie.addSeries("licenses", data);
		licensePie.render();
		//var licensePieLegend = new dojox.charting.widget.Legend({chart: licensePie}, legendId);
	});
}

getLicenceFileName = function(name) {
	name = name.toLowerCase();
	name = name.replace(new RegExp(" ", "g"), '')
	name = name.replace(new RegExp("-", "g"), '')
	name = name.replace(new RegExp(":", "g"), '')
	name = name.replace(new RegExp("\\(", "g"), '')
	name = name.replace(new RegExp("\\)", "g"), '')
	name = name.replace(new RegExp("\\\\", "g"), '')
	name = name.replace(new RegExp("\\/", "g"), '')
	name = name.replace(new RegExp("\\_", "g"), '')
	name = name.replace(new RegExp("gplcompatible", "g"), '')
	name = name.replace(new RegExp("gnugeneralpubliclicense", "g"), 'gpl-')
	name = name.replace(new RegExp("generalpubliclicense", "g"), 'gpl-')
	name = name.replace(new RegExp("creativecommons", "g"), 'cc-')
	name = name.replace(new RegExp("attribution", "g"), 'by-')
	name = name.replace(new RegExp("sharealike", "g"), 'sa-')
	name = name.replace(new RegExp("license", "g"), '')
	name = name.replace(new RegExp("and", "g"), '&')
	name = name.replace(new RegExp("\\.", "g"), '')
	name = name.replace(new RegExp(" ", "g"), '')
	name = name.replace(new RegExp("&", "g"), '_')
	return "license-" + name + ".html";
}

function updateChartes(dir, data) {
	updateLicensePieData("licensePie", "licensePieLegend", data);

	var node = dojo.byId("history");
	node.src = dir + "plot.png";
	var node = dojo.byId("nonfreehistory");
	node.src = dir + "nonfree.png";

	var node = dojo.byId("brutlist");
	while (node.hasChildNodes()) {
		node.removeChild(node.firstChild);
	}

	var list = [];
	for (l in data) {
		list.push([l, data[l]]);
	}
	var comparator = function(a, b) {
		return (a[1] < b[1]);
	};
	list.sort(comparator);

	var text = "<ul>";
	for (l in list) {
		l = list[l][0];
		var number = data[l];
		var url = dir + getLicenceFileName(l);
		var style = "";
		if (isNonFreeLicense(l))
			style = " style=\"badlicense\"";
		text += "<li>" + number + " - <a " + style + "href=\"" + url + "\">" + l + "</a></li>";
	}
	text += "</ul>";
	node.innerHTML = text;
}

function invokeCharteUpdate(dir) {
	var dir = (dir + "/").replace("base/", "");
	chartdata = dir + "chartdata.json";
	dojo.xhrGet({
	    url: chartdata,
	    handleAs: "json",
	    //headers: {"Origin": "file://"},	// unfixable bug with Chromium
	    load: function(data) {
		updateChartes(dir, data)
	    }
	});
}
