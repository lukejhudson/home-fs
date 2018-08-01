<?php
$total = count($_FILES["file_to_upload"]["name"]);
$path = $_GET["path"];
$target_dir = ".." . $path . "/";
echo $target_dir . "<br>";

for ($i = 0; $i < $total; $i++) {
	$target_file = $target_dir . basename($_FILES["file_to_upload"]["name"][$i]);
	echo $target_file . "<br>";
	$uploadOk = 1;
	$error = "";
	
	if ($_FILES["file_to_upload"]["name"][$i] == "") {
		$error = "Please choose a file to upload\n";
		$uploadOk = 0;
	}
	// Check if file already exists
	else if (file_exists($target_file)) {
		$error = "File already exists\n";
		$uploadOk = 0;
	}
	// Check if file is too large - 8388608 max
	/*
	else if ($_FILES["file_to_upload"]["size"][$i] > 8380000) {
		$error = "File too large\n";
		$uploadOk = 0;
	}
	*/
	// Check if $uploadOk is set to 0 by an error
	if ($uploadOk == 0) {
	    echo "<b>Error</b> File was not uploaded: " . $error . "<br>";
	// If everything is ok, try to upload file
	} else {
		if (move_uploaded_file($_FILES["file_to_upload"]["tmp_name"][$i], $target_file)) {
			echo "<b>Success</b> The file " . basename($_FILES["file_to_upload"]["name"][$i]) . " has been uploaded<br>";
		} else {
			echo "<b>Error</b> File was not uploaded: An unknown error occurred\n";
		}
	}
}

// print_r($_FILES);

?>

