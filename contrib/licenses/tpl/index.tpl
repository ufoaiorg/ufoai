<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:py="http://genshi.edgewall.org/"
      xmlns:xi="http://www.w3.org/2001/XInclude">
<py:match path="content">

<h3>Subdirectories</h3>
${_show(sub)}

<h3>licenses</h3>
${_show(licenses)}

</py:match>
<xi:include href="main.tpl" />
</html>
