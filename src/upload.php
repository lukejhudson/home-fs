<?php
$target_dir = "../uploads/";
$total = count($_FILES["file_to_upload"]["name"]);

for ($i = 0; $i < $total; $i++) {
	$target_file = $target_dir . basename($_FILES["file_to_upload"]["name"][$i]);
	$uploadOk = 1;
	// Check if file already exists
	if (file_exists($target_file)) {
	    echo "Sorry, file already exists.\n";
	    $uploadOk = 0;
	}
	// Check if $uploadOk is set to 0 by an error
	if ($uploadOk == 0) {
	    echo "Sorry, your file was not uploaded.\n";
	// Ff everything is ok, try to upload file
	} else {
	    if (move_uploaded_file($_FILES["file_to_upload"]["tmp_name"][$i], $target_file)) {
		echo "The file " . basename($_FILES["file_to_upload"]["name"][$i]) . " has been uploaded.\n";
	    } else {
		echo "Sorry, there was an error uploading your file.\n";
	    }
	}
}

print_r($_FILES);

?>

