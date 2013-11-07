---
layout: post
title: "Looking for a blog engine"
date: 2013-11-06 23:58
comments: true
categories: 
---

You are reading our blog. Good. So I should better tell you what this is all about. Sebastian and I are working on our master theses and we are writing a blog along the way. That way, we can publish some information early and often - and it is a good opportunity to practise our English writing skills.

I guess now you wonder what the topic of our theses is. Sorry, you have to wait for our next posts. This time, I'm writing about a more mundane topic: We need some blog engine.

When I asked some friends, they said: "That's easy - don't write a blog." Well, I'm doing this for my master thesis. Then they suggested HTML and vi. Not so helpful...

So how can we find a good way to write a blog? Well, obviously there is Wordpress. I was suprised that even a lot of techical blogs use it. However, it grew into much more than a simple blog engine, so this is not what I was looking for.

During my search, I looked at the source of lot of blogs to find out what they are using. Wikipedia lists software that is used by top 20 blogs (see [here](http://en.wikipedia.org/wiki/Blog_software#Software_used_by_top_20_blogs)) - mostly Wordpress. I like [Movable type](http://www.movabletype.com/blog.html), but it costs too much money.

Another intersting one is Blogger because it was funded by Y Combinator. I really love Paul Graham's books about Lisp. He certainly is a smart guy. So I figured that a project that is supported by his company is worth a look. Well, they've been bought by Google and the site is closed. That certainly was an interesting thing, if Google was so interested in it ;-)

Then, I found [Ghost](https://ghost.org/). It doesn't have any bells and whistles that we don't need. Even better, it uses Markdown for the posts. I'm using Markdown a lot - for personal notes, for notes in lectures, when I'm writing for the [lessproject wiki](http://lessproject.de/). I don't have to tell you that I immediately liked Ghost. After some struggles with the Node package manager ([solution is here](http://stackoverflow.com/a/18428563)), I have the blog up and running.

**Update:**

Ghost only lasted for about three days. Today, a friend told me that he uses [Octopress](http://octopress.org/) for blogging. The workflow is quite different from Ghost: You write the pages on your PC, let Octopress generate the HTML and push it to a web server. This means that we can use Github pages to host the blog. This is a huge advantage because it will last. The Ghost blog is hosted on my private server, so it won't last forever. If anyone uses our software, I don't want them to be stopped (or hindered) by a dead page. The Github pages will live as long as the code repository.

We will use a git to store the Markdown source of our posts. We use a similar workflow for source code and notes. The git will track who writes posts, but this won't show up in the posts. Therefore, we will add a signature at the end of each post. Feel free to remind us, if we forget to do that.

-- Benjamin
