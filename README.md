# RAIN - Resident Audio Interface

![](DOC/rain.jpg?raw=true)

RAIN aims to provide high-quality high-level audio services (like playing xm, mp3..)
to wide range of applications, especially in cases where all other techniques fail:

- DOS programs run under Windows 
     (problem: Windows tend to hide soundcard ports; and ports could be quite non-standard anyway and useful only via Windows driver)
- 16bit DOS real or protected mode programs
     (problem: there are no good 16bit audio libraries)

RAIN achieves this by separating audio server from client, so that both can run in different memory models or even OSes.

There are precompiled audio servers for DOS and Windows and nice high-level C/C++ and Pascal API to access them from your clients. Other platforms and languages can be added. Sample clients 
(<a href="https://rawgit.com/StepanHrbek/RAIN/SRC/DEMO-C/PLAY.C">C sample</a>, <a href="https://rawgit.com/StepanHrbek/RAIN/SRC/DEMO-PAS/PLAY.PAS">Pascal sample</a>) work with all 12 tested compilers.

<a href="https://rawgit.com/StepanHrbek/RAIN/master/DOC/index.html">**documentation**</a>
