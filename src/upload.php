<?php
$target_dir = "../uploads/";
$total = count($_FILES["file_to_upload"]["name"]);

for ($i = 0; $i < $total; $i++) {
	$target_file = $target_dir . basename($_FILES["file_to_upload"]["name"][$i]);
	$uploadOk = 1;
	$error = "";
	// Check if file already exists
	if (file_exists($target_file)) {
		$error = "File already exists\n";
		$uploadOk = 0;
	}
	// Check if file is too large - 8388608 max
	if ($_FILES["file_to_upload"]["size"][$i] > 8380000) {
		$error = "File too large\n";
		$uploadOk = 0;
	}
	
	// Check if $uploadOk is set to 0 by an error
	if ($uploadOk == 0) {
	    echo "<b>Error</b> File was not uploaded: " . $error;
	// Ff everything is ok, try to upload file
	} else {
		if (move_uploaded_file($_FILES["file_to_upload"]["tmp_name"][$i], $target_file)) {
			echo "<b>Success</b> The file " . basename($_FILES["file_to_upload"]["name"][$i]) . " has been uploaded\n";
		} else {
			echo "<b>Error</b> File was not uploaded: An unknown error occurred\n";
		}
	}
}

// print_r($_FILES);

?>

