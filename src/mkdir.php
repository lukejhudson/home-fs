<?php

$name = $_POST["name"];
$path = $_GET["path"];

if (mkdir(".." . $path . "/" . $name)) {
	echo "<b>Success</b>: Directory " . $name . " has been created";
} else {
	echo "<b>Error</b>: Could not create directory " . $name;
}
?>

