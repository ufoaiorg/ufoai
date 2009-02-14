<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:py="http://genshi.edgewall.org/"
      xmlns:xi="http://www.w3.org/2001/XInclude"
      py:strip="True">
<py:match path="content">

<h3>$license</h3>

<ol>
 <li py:for="f in files">${Markup(f.show())}</li>
</ol>

</py:match>
<xi:include href="main.tpl" />
</html>
