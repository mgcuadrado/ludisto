workdir=work

drawings.pdf: drawings.tex test.settings
	rubber --force --unsafe -d $<

svg:
	./gensvg $(workdir)

install: svg
	cp $(workdir)/*.svg ../share

clean:
	rm -rf $(workdir)
