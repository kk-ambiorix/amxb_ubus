include ../../makefile.inc

ALIEN = $(shell which alien)
FAKEROOT = $(shell which fakeroot)

NOW = $(shell date +"%a, %d %b %Y %H:%M:%S %z")
PKGDIR = ../../output/$(MACHINE)/
DEBDIR = $(PKGDIR)$(COMPONENT)-$(VERSION)/debian
PACKAGE_TAR_GZ = $(COMPONENT)-$(VERSION).tar.gz
LOG = $(shell $(GIT) log $$($(GIT) describe --tags | cut -d'-' -f1)..HEAD)
ARCHITECTURE = $(shell dpkg-architecture | grep DEB_TARGET_ARCH= | cut -d'=' -f2)

PACKAGE_DEB = $(COMPONENT)_$(VERSION)_$(ARCHITECTURE).deb

AFLAGS = -d -k --fixperms --test --description="Generic C-implemnentation of data containers" \
         --version="$(VERSION)" --target="$(ARCHITECTURE)" -g -v 

# helper functions - used in multiple targets
# 1 - destination dir
# 2 - filename
define create_changelog
	@$(ECHO) "Generating debian changelog from git log"
	$(ECHO) "$(COMPONENT) ($(VERSION)) unstable; urgency=medium" > $(1)/$(2)
	$(ECHO) "" >> $(1)/$(2)
	$(GIT) log --pretty=format:"  * %s" $$($(GIT) describe --tags | grep -v "merge" | cut -d'-' -f1)..HEAD  >> $(1)/$(2)
	$(ECHO) "" >> $(1)/$(2)
	$(ECHO) "" >> $(1)/$(2)
	$(GIT) log --pretty=format:" -- %an <%ae>  $(NOW)" -1 >> $(1)/$(2)
	$(ECHO) "" >> $(1)/$(2)
	$(ECHO) "" >> $(1)/$(2)
	$(CAT) changelog >> $(1)/$(2)
endef

all: $(PKGDIR)$(PACKAGE_DEB)
	$(INSTALL) -m 0644 $(PKGDIR)$(PACKAGE_DEB) ../../

$(PKGDIR)$(PACKAGE_DEB): $(DEBDIR)/ changelog
	@$(ECHO) "Install debian control files"
	$(INSTALL) -m 0644 control $(DEBDIR)
	$(INSTALL) -m 0644 copyright $(DEBDIR)
	$(INSTALL) -m 0644 triggers $(DEBDIR)
	$(INSTALL) -m 0644 compat $(DEBDIR)
	$(INSTALL) -m 0644 rules $(DEBDIR)
	$(ECHO) "Build debian package"
	cd $(DEBDIR)/.. && $(FAKEROOT) make -f debian/rules binary

changelog: $(DEBDIR)/
ifneq ($(LOG),)
	$(call create_changelog,$(DEBDIR),changelog) 
else
	$(INSTALL) -m 0644 changelog $(DEBDIR)
endif

$(DEBDIR)/: $(PKGDIR)$(PACKAGE_TAR_GZ)
	rm -rf $(PKGDIR)/$(COMPONENT)-$(VERSION)
	cd $(PKGDIR) && $(FAKEROOT) $(ALIEN) $(AFLAGS) $(PACKAGE_TAR_GZ)

update_changelog:
	$(call create_changelog,.,changelog.tmp) 
	mv changelog.tmp changelog

.PHONY: all changelog update_changelog