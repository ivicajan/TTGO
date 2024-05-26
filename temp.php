<?php
# Assigning the ini-values to usable variables
$db_host = "localhost";
$db_name = "mydb";
$db_table = "sensors";
$db_user = "python_logger";
$db_password = "sifra";

# Prepare a connection to the mySQL database
$connection = new mysqli($db_host, $db_user, $db_password, $db_name);

?>
<!-- start of the HTML part that Google Chart needs -->
<html>
<head>
	<script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
	<link rel="shortcut icon"  href="http://boreas.mywire.org/temp.ico">
<!-- This loads the 'corechart' package. -->	
    <script type="text/javascript">
    	google.charts.load('current', {'packages':['corechart']});
     	google.charts.setOnLoadCallback(drawChart);

		function drawChart() {
    		var data1 = google.visualization.arrayToDataTable([			
			['Time', 'Deck'],
<?php
#$sql = "SELECT id, name, value, timestamp FROM $db_table where id mod 6 = 0 AND name = 'juraj_soba/temp' ORDER BY id ASC LIMIT 144";
$sql = "SELECT id, name, value, timestamp FROM $db_table where name = 'deck/temp' ORDER BY id DESC LIMIT 288";
#$sql = "SELECT id, name, value, timestamp FROM $db_table where name = 'rapi3_temp/temp' ORDER BY id DESC LIMIT 432";
$result = $connection->query($sql);  
# This while - loop formats and put all the retrieved data into ['timestamp', 'temperature'] way.
	while ($row = $result->fetch_assoc()) {
		#$timestamp_rest = substr($row["timestamp"],-8);
		#echo "['".$timestamp_rest."',".$row['value']."],";
		echo "['".$row['timestamp']."',".$row['value']."],";
		}
?>
]);
    		var data2 = google.visualization.arrayToDataTable([			
			['Time', 'Living room'],
<?php
$sql = "SELECT id, name, value, timestamp FROM $db_table where name = 'dnevna_soba/temp' ORDER BY id DESC LIMIT 288";
#$sql = "SELECT id, name, value, timestamp FROM $db_table where name = 'rapi3_temp/temp' ORDER BY id DESC LIMIT 432";
$result = $connection->query($sql);  
# This while - loop formats and put all the retrieved data into ['timestamp', 'temperature'] way.
	while ($row = $result->fetch_assoc()) {
		#$timestamp_rest = substr($row["timestamp"],-8);
		#echo "['".$timestamp_rest."',".$row['value']."],";
		echo "['".$row['timestamp']."',".$row['value']."],";
		}
?>
]);

    var joinedData = google.visualization.data.join(data1, data2, 'full', [[0, 0]], [1], [1]);

// Curved line
var options = {
		title: 'Temperature ESP32 project',
		titleTextStyle: {fontSize: 18},
		curveType: 'function',
		legend: { position: 'bottom' },
		interpolateNulls: true,
		lineWidth: 0.1,
		crosshair: { trigger: 'both' },
		animation:{"startup": true}, 
		chartArea: {backgroundColor: {'fill': '#F4F4F4','opacity': 100},},
	//	hAxis: {textStyle :{fontSize: 10}, slantedText:true, slantedTextAngle:90},
		hAxis: {slantedText: false, textStyle :{fontSize: 14}},
		vAxis: {slantedText: false, gridlines: {'count': -1}, textStyle :{fontSize: 14}},
		};

//		title: 'Temperature fun project',
//		hAxis: {title: 'Year',  titleTextStyle: {color: '#333'}},
//		vAxis: {minValue: 10}
//		};
// Curved chart
//var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));
//var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));
var chart = new google.visualization.AreaChart(document.getElementById('curve_chart'));
//var chart = new google.visualization.SteppedAreaChart(document.getElementById('curve_chart'));
chart.draw(joinedData, options);

} // End bracket from drawChart
</script>
</head>
<! -- <div id="curve_chart" style="width: 1200px; height: 600px;"></div>  -->
<div id="curve_chart" style="  position: absolute; top: 0; left: 0; height: 100%;  width: 100%;" >
  </div>
</html>

