<?php
// http://php.net/manual/en/function.rmdir.php
// Recursively delete all files and directories inside the directory, then the directory itself
function delTree($dir) {
	$files = array_diff(scandir($dir), array('.','..'));
	foreach ($files as $file) {
		(is_dir("$dir/$file")) ? delTree("$dir/$file") : unlink("$dir/$file");
	}
	return rmdir($dir);
} 

$file = $_GET["file"];
$path = ".." . $_GET["path"];

// Check that $path starts with /uploads
// Go into directory $path, delete file/dir $file
if (is_dir("$path/$file")) { // Delete directory
	if (delTree("$path/$file")) {
		echo "<b>Success</b>: Directory " . $file . " has been deleted";
	} else {
		echo "<b>Error</b>: Could not delete directory " . $file;
	}
} else { // Delete file
	if (unlink("$path/$file")) {
		echo "<b>Success</b>: File " . $file . " has been deleted";
	} else {
		echo "<b>Error</b>: Could not delete file " . $file;
	}
}
?>

