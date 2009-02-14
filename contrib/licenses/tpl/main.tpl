<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:py="http://genshi.edgewall.org/"
      xmlns:xi="http://www.w3.org/2001/XInclude"
      py:strip="True">
<head>
 <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
 <style  type="text/css">
body { color: #ffffff; background-color: #262626; font-family: verdana, helvetica, arial, sans-serif; padding: 0; margin: 10px;}
html > body { font-size: 8.5pt; }
a { color: #ffd800; background-color: transparent; text-decoration: none; }
a:hover { color: #ffffff; background-color: transparent; text-decoration: none; }
li { margin-bottom: 8px; }
.author { }
 </style>
 <title>ufo::licenses $path</title>
</head>

<body>
 <div>
  <a href="index"> / </a>
  <a py:for="p in path.split('/')[:-1]" href="index?path=$p"> $p/ </a>  ${path.split('/')[-1]}
 </div>

 <content />
</body>
</html>
