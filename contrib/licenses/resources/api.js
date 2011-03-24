
function getShortLicenseName(license) {
	license = license.replace("GNU General Public License", "GPL");
	license = license.replace("Creative Commons", "CC");
	license = license.replace("Attribution", "by");
	license = license.replace("Share Alike", "SA");
	return license;
}

function initializeLicensePieLater(chartId, legendId) {
	dojo.require("dojox.charting.Chart2D");
	dojo.require("dojox.charting.plot2d.Pie");
	dojo.require("dojox.charting.action2d.Highlight");
	dojo.require("dojox.charting.action2d.MoveSlice");
	dojo.require("dojox.charting.action2d.Tooltip");
	dojo.require("dojox.charting.themes.MiamiNice");
	dojo.require("dojox.charting.widget.Legend");

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
		var dc = dojox.charting;
		var licensePie = new dc.Chart2D(chartId);
		licensePie.setTheme(dc.themes.MiamiNice).addPlot("default", {
			type: "Pie",
			font: "normal normal 11pt Tahoma",
			fontColor: "black",
			labelOffset: -30,
			radius: 80
		});
		licensePie.addSeries("licenses", data);
		var anim_a = new dc.action2d.MoveSlice(licensePie, "default");
		var anim_b = new dc.action2d.Highlight(licensePie, "default");
		var anim_c = new dc.action2d.Tooltip(licensePie, "default");
		licensePie.render();
		//var licensePieLegend = new dojox.charting.widget.Legend({chart: licensePie}, legendId);
	});
}
